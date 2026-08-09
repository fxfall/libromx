#include "romx_internal.h"

#include <string.h>

static uint32_t romx_read_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0]
        | ((uint32_t)bytes[1] << 8)
        | ((uint32_t)bytes[2] << 16)
        | ((uint32_t)bytes[3] << 24);
}

static uint64_t romx_read_le64(const uint8_t *bytes)
{
    uint64_t value = UINT64_C(0);
    unsigned int index;

    for (index = 0U; index < 8U; ++index) {
        value |= (uint64_t)bytes[index] << (index * 8U);
    }
    return value;
}

static int romx_region_is_valid(romx_region_info_t region, uint64_t body_size)
{
    if (region.size == UINT64_C(0)) {
        return 1;
    }
    return region.offset <= body_size && region.size <= body_size - region.offset;
}

static int romx_regions_overlap(romx_region_info_t first, romx_region_info_t second)
{
    uint64_t first_end;
    uint64_t second_end;

    if (first.size == UINT64_C(0) || second.size == UINT64_C(0)) {
        return 0;
    }
    first_end = first.offset + first.size;
    second_end = second.offset + second.size;
    return first.offset < second_end && second.offset < first_end;
}

/* Every byte before the footer belongs to exactly one non-empty region. */
static int romx_regions_cover_body(const romx_info_t *info)
{
    romx_region_info_t regions[3];
    size_t count = 0U;
    size_t index;
    uint64_t cursor = UINT64_C(0);

    if (info->rom.size != UINT64_C(0)) regions[count++] = info->rom;
    if (info->metadata.size != UINT64_C(0)) regions[count++] = info->metadata;
    if (info->cover.size != UINT64_C(0)) regions[count++] = info->cover;
    for (index = 1U; index < count; ++index) {
        romx_region_info_t value = regions[index];
        size_t position = index;
        while (position > 0U && regions[position - 1U].offset > value.offset) {
            regions[position] = regions[position - 1U];
            --position;
        }
        regions[position] = value;
    }
    for (index = 0U; index < count; ++index) {
        if (regions[index].offset != cursor) return 0;
        cursor += regions[index].size;
    }
    return cursor == info->body_size;
}

static int romx_bytes_are_zero(const uint8_t *bytes, size_t size)
{
    size_t index;

    for (index = 0U; index < size; ++index) {
        if (bytes[index] != 0U) {
            return 0;
        }
    }
    return 1;
}

romx_result_t romx_parse_footer(
    const uint8_t footer[ROMX_FOOTER_SIZE_0_1_0],
    uint64_t file_size,
    romx_info_t *info,
    romx_error_t *error)
{
    uint32_t flags;
    uint64_t footer_offset;

    if (footer == NULL || info == NULL) {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN, "footer and info must not be null");
    }
    if (file_size < ROMX_FOOTER_SIZE_0_1_0) {
        return romx_error_set(error, ROMX_E_TRUNCATED, 0, file_size,
            "file is shorter than the ROMX 0.1.0 footer");
    }

    footer_offset = file_size - ROMX_FOOTER_SIZE_0_1_0;
    if (memcmp(footer, "ROMX", 4U) != 0) {
        return romx_error_set(error, ROMX_E_INVALID_FOOTER, 0, footer_offset,
            "footer magic is not ROMX");
    }

    memset(info, 0, sizeof(*info));
    info->struct_size = (uint32_t)sizeof(*info);
    info->version = romx_read_le32(footer + 0x04U);
    if (info->version != ROMX_FORMAT_VERSION_0_1_0) {
        return romx_error_set(error, ROMX_E_INVALID_FOOTER, 0,
            footer_offset + 0x04U, "unsupported ROMX version");
    }

    info->file_size = file_size;
    info->body_size = footer_offset;
    info->rom.offset = romx_read_le64(footer + 0x08U);
    info->rom.size = romx_read_le64(footer + 0x10U);
    info->metadata.offset = romx_read_le64(footer + 0x18U);
    info->metadata.size = romx_read_le64(footer + 0x20U);
    info->cover.offset = romx_read_le64(footer + 0x28U);
    info->cover.size = romx_read_le64(footer + 0x30U);
    /* An absent region has no address. Do not expose or use its stale offset. */
    if (info->metadata.size == UINT64_C(0)) info->metadata.offset = UINT64_C(0);
    if (info->cover.size == UINT64_C(0)) info->cover.offset = UINT64_C(0);
    memcpy(info->reserved, footer + 0x38U, sizeof(info->reserved));
    info->flags = romx_read_le32(footer + 0x58U);
    info->footer_size = romx_read_le32(footer + 0x5CU);
    memcpy(info->body_sha256, footer + 0x60U, sizeof(info->body_sha256));

    if (info->footer_size != ROMX_FOOTER_SIZE_0_1_0) {
        return romx_error_set(error, ROMX_E_INVALID_FOOTER, 0,
            footer_offset + 0x5CU, "ROMX 0.1.0 footer_size must be 128");
    }

    flags = info->flags;
    if ((flags & ~ROMX_FLAGS_0_1_0_MASK) != UINT32_C(0)) {
        return romx_error_set(error, ROMX_E_INVALID_FLAGS, 0,
            footer_offset + 0x58U, "ROMX 0.1.0 reserved flags must be zero");
    }
    if (((flags & ROMX_FLAG_HAS_METADATA) != 0U) !=
            (info->metadata.size != UINT64_C(0)) ||
        ((flags & ROMX_FLAG_HAS_COVER) != 0U) !=
            (info->cover.size != UINT64_C(0))) {
        return romx_error_set(error, ROMX_E_INVALID_FLAGS, 0,
            footer_offset + 0x58U, "footer flags do not match optional region sizes");
    }
    if ((flags & ROMX_FLAG_HAS_BODY_SHA256) == 0U &&
        !romx_bytes_are_zero(info->body_sha256, sizeof(info->body_sha256))) {
        return romx_error_set(error, ROMX_E_INVALID_FLAGS, 0,
            footer_offset + 0x60U,
            "body_sha256 must be zero when HAS_BODY_SHA256 is not set");
    }

    if (info->rom.size == UINT64_C(0)) {
        return romx_error_set(error, ROMX_E_INVALID_FOOTER, 0,
            footer_offset + 0x10U, "ROM payload must not be empty");
    }
    if (!romx_region_is_valid(info->rom, info->body_size)) {
        return romx_error_set(error, ROMX_E_RANGE, 0,
            footer_offset + 0x08U, "ROM region exceeds the container body");
    }
    if (!romx_region_is_valid(info->metadata, info->body_size)) {
        return romx_error_set(error, ROMX_E_RANGE, 0,
            footer_offset + 0x18U, "metadata region exceeds the container body");
    }
    if (!romx_region_is_valid(info->cover, info->body_size)) {
        return romx_error_set(error, ROMX_E_RANGE, 0,
            footer_offset + 0x28U, "cover region exceeds the container body");
    }

    if (romx_regions_overlap(info->rom, info->metadata)) {
        return romx_error_set(error, ROMX_E_OVERLAP, 0,
            footer_offset + 0x18U, "ROM and metadata regions overlap");
    }
    if (romx_regions_overlap(info->rom, info->cover)) {
        return romx_error_set(error, ROMX_E_OVERLAP, 0,
            footer_offset + 0x28U, "ROM and cover regions overlap");
    }
    if (romx_regions_overlap(info->metadata, info->cover)) {
        return romx_error_set(error, ROMX_E_OVERLAP, 0,
            footer_offset + 0x28U, "metadata and cover regions overlap");
    }
    if (!romx_regions_cover_body(info)) {
        return romx_error_set(error, ROMX_E_RANGE, 0,
            footer_offset, "footer body contains uncovered bytes");
    }

    romx_error_clear(error);
    return ROMX_OK;
}
