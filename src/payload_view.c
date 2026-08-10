#include "romx_internal.h"

#include <stddef.h>
#include <stdlib.h>

static romx_result_t romx_payload_get_size(
    void *user_data,
    uint64_t *size,
    romx_error_t *error)
{
    const romx_reader_t *reader = (const romx_reader_t *)user_data;

    romx_error_clear(error);
    if (reader == NULL || size == NULL) {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN, "payload view and size must not be null");
    }
    *size = reader->info.rom.size;
    return ROMX_OK;
}

static romx_result_t romx_payload_read_at(
    void *user_data,
    uint64_t offset,
    void *buffer,
    uint64_t size,
    uint64_t *bytes_read,
    romx_error_t *error)
{
    const romx_reader_t *reader = (const romx_reader_t *)user_data;
    uint64_t count;
    romx_result_t result;

    romx_error_clear(error);
    if (bytes_read != NULL) {
        *bytes_read = UINT64_C(0);
    }
    if (reader == NULL || bytes_read == NULL ||
        (buffer == NULL && size != UINT64_C(0)) ||
        size > (uint64_t)SIZE_MAX) {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN, "invalid payload view read arguments");
    }

    /* Match regular-file EOF semantics, including offsets beyond EOF. */
    if (size == UINT64_C(0) || offset >= reader->info.rom.size) {
        return ROMX_OK;
    }
    count = reader->info.rom.size - offset;
    if (count > size) {
        count = size;
    }

    result = reader->io.read_at(reader->io.user_data,
        reader->info.rom.offset + offset, buffer, count, bytes_read, error);
    if (result != ROMX_OK) {
        return result;
    }
    if (*bytes_read != count) {
        return romx_error_set(error, ROMX_E_TRUNCATED, 0,
            reader->info.rom.offset + offset + *bytes_read,
            "ROMX payload view read was truncated");
    }
    return ROMX_OK;
}

romx_result_t romx_reader_get_payload_io(
    const romx_reader_t *reader,
    romx_io_t *out_io,
    romx_error_t *error)
{
    uint32_t supplied_size;

    romx_error_clear(error);
    if (reader == NULL || out_io == NULL) {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN, "reader and payload I/O must not be null");
    }
    supplied_size = out_io->struct_size;
    if (supplied_size < sizeof(*out_io)) {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN, "payload I/O structure is too small");
    }

    out_io->struct_size = supplied_size;
    out_io->user_data = (void *)reader;
    out_io->get_size = romx_payload_get_size;
    out_io->read_at = romx_payload_read_at;
    return ROMX_OK;
}

romx_result_t romx_reader_map_payload(
    const romx_reader_t *reader,
    romx_payload_mapping_t **out_mapping,
    romx_error_t *error)
{
    romx_result_t result;

    romx_error_clear(error);
    if (out_mapping != NULL) {
        *out_mapping = NULL;
    }
    if (reader == NULL || out_mapping == NULL) {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN, "reader and output mapping must not be null");
    }
    if (reader->map_payload == NULL) {
        return romx_error_set(error, ROMX_E_UNSUPPORTED, 0,
            ROMX_OFFSET_UNKNOWN, "payload mapping is unavailable for this input source");
    }

    /* The optional body hash is normative when present. With the default
     * flag state this is a constant-time no-op and never scans the payload. */
    result = romx_validate_required_integrity(reader, error);
    if (result != ROMX_OK) {
        return result;
    }
    return reader->map_payload(reader->io.user_data, reader->info.rom,
        out_mapping, error);
}

const void *romx_payload_mapping_data(const romx_payload_mapping_t *mapping)
{
    return mapping != NULL ? mapping->data : NULL;
}

uint64_t romx_payload_mapping_size(const romx_payload_mapping_t *mapping)
{
    return mapping != NULL ? mapping->size : UINT64_C(0);
}

void romx_payload_mapping_close(romx_payload_mapping_t *mapping)
{
    if (mapping == NULL) {
        return;
    }
    if (mapping->release != NULL) {
        mapping->release(mapping);
    }
    free(mapping);
}
