#include <romx/romx.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct memory_io {
    const uint8_t *data;
    uint64_t size;
    int truncate_footer;
} memory_io_t;

typedef struct virtual_io {
    uint8_t footer[ROMX_FOOTER_SIZE_V1];
    uint64_t size;
} virtual_io_t;

static int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", \
            __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static void write_le32(uint8_t *bytes, uint32_t value)
{
    unsigned int index;
    for (index = 0U; index < 4U; ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8U));
    }
}

static void write_le64(uint8_t *bytes, uint64_t value)
{
    unsigned int index;
    for (index = 0U; index < 8U; ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8U));
    }
}

static void make_footer(
    uint8_t *footer,
    uint64_t rom_offset,
    uint64_t rom_size,
    uint64_t metadata_offset,
    uint64_t metadata_size,
    uint64_t cover_offset,
    uint64_t cover_size,
    uint32_t flags)
{
    memset(footer, 0, ROMX_FOOTER_SIZE_V1);
    memcpy(footer, "ROMX", 4U);
    write_le32(footer + 0x04U, ROMX_FORMAT_VERSION_1);
    write_le64(footer + 0x08U, rom_offset);
    write_le64(footer + 0x10U, rom_size);
    write_le64(footer + 0x18U, metadata_offset);
    write_le64(footer + 0x20U, metadata_size);
    write_le64(footer + 0x28U, cover_offset);
    write_le64(footer + 0x30U, cover_size);
    write_le32(footer + 0x58U, flags);
    write_le32(footer + 0x5CU, ROMX_FOOTER_SIZE_V1);
}

static romx_result_t memory_get_size(
    void *user_data,
    uint64_t *size,
    romx_error_t *error)
{
    memory_io_t *memory = (memory_io_t *)user_data;
    (void)error;
    *size = memory->size;
    return ROMX_OK;
}

static romx_result_t memory_read_at(
    void *user_data,
    uint64_t offset,
    void *buffer,
    uint64_t size,
    uint64_t *bytes_read,
    romx_error_t *error)
{
    memory_io_t *memory = (memory_io_t *)user_data;
    uint64_t available;
    uint64_t count;
    (void)error;

    if (offset > memory->size) {
        *bytes_read = UINT64_C(0);
        return ROMX_OK;
    }
    available = memory->size - offset;
    count = size < available ? size : available;
    if (memory->truncate_footer && count != UINT64_C(0)) {
        --count;
    }
    memcpy(buffer, memory->data + (size_t)offset, (size_t)count);
    *bytes_read = count;
    return ROMX_OK;
}

static romx_result_t open_memory(
    memory_io_t *memory,
    romx_reader_t **reader,
    romx_error_t *error)
{
    romx_io_t io = ROMX_IO_INIT;
    io.user_data = memory;
    io.get_size = memory_get_size;
    io.read_at = memory_read_at;
    return romx_reader_open_io(&io, NULL, reader, error);
}

static romx_result_t virtual_get_size(
    void *user_data,
    uint64_t *size,
    romx_error_t *error)
{
    virtual_io_t *virtual_file = (virtual_io_t *)user_data;
    (void)error;
    *size = virtual_file->size;
    return ROMX_OK;
}

static romx_result_t virtual_read_at(
    void *user_data,
    uint64_t offset,
    void *buffer,
    uint64_t size,
    uint64_t *bytes_read,
    romx_error_t *error)
{
    virtual_io_t *virtual_file = (virtual_io_t *)user_data;
    (void)error;
    if (offset != virtual_file->size - ROMX_FOOTER_SIZE_V1 ||
        size != ROMX_FOOTER_SIZE_V1) {
        *bytes_read = UINT64_C(0);
        return ROMX_E_IO;
    }
    memcpy(buffer, virtual_file->footer, ROMX_FOOTER_SIZE_V1);
    *bytes_read = ROMX_FOOTER_SIZE_V1;
    return ROMX_OK;
}

static void expect_result(memory_io_t *memory, romx_result_t expected)
{
    romx_reader_t *reader = NULL;
    romx_error_t error;
    const romx_result_t actual = open_memory(memory, &reader, &error);
    CHECK(actual == expected);
    CHECK(error.code == expected || actual == ROMX_OK);
    if (actual != ROMX_OK) {
        CHECK(error.message[0] != '\0');
    }
    romx_reader_close(reader);
}

static void test_valid_canonical(void)
{
    uint8_t file[160U + ROMX_FOOTER_SIZE_V1];
    memory_io_t memory = { file, sizeof(file), 0 };
    romx_reader_t *reader = NULL;
    romx_error_t error;
    romx_info_t info = ROMX_INFO_INIT;

    memset(file, 0xA5, 160U);
    make_footer(file + 160U, 0U, 100U, 100U, 40U, 140U, 20U,
        ROMX_FLAG_HAS_METADATA | ROMX_FLAG_HAS_COVER | ROMX_FLAG_HAS_BODY_SHA256);
    memset(file + 160U + 0x60U, 0x5A, 32U);

    CHECK(open_memory(&memory, &reader, &error) == ROMX_OK);
    CHECK(reader != NULL);
    CHECK(romx_reader_get_info(reader, &info, &error) == ROMX_OK);
    CHECK(info.version == ROMX_FORMAT_VERSION_1);
    CHECK(info.file_size == sizeof(file));
    CHECK(info.body_size == 160U);
    CHECK(info.rom.offset == 0U && info.rom.size == 100U);
    CHECK(info.metadata.offset == 100U && info.metadata.size == 40U);
    CHECK(info.cover.offset == 140U && info.cover.size == 20U);
    CHECK(info.body_sha256[0] == 0x5AU);
    romx_reader_close(reader);
}

static void test_valid_reordered_and_empty_offsets(void)
{
    uint8_t file[64U + ROMX_FOOTER_SIZE_V1];
    memory_io_t memory = { file, sizeof(file), 0 };

    memset(file, 0, sizeof(file));
    make_footer(file + 64U, 32U, 16U, UINT64_MAX, 0U, 0U, 8U,
        ROMX_FLAG_HAS_COVER);
    /* Empty metadata offset is ignored, but the non-empty regions leave a
       gap in the body and are rejected by the complete-coverage rule. */
    expect_result(&memory, ROMX_E_RANGE);
}

static void test_identity_and_fixed_fields(void)
{
    uint8_t file[16U + ROMX_FOOTER_SIZE_V1];
    memory_io_t memory = { file, sizeof(file), 0 };
    uint8_t *footer = file + 16U;

    memset(file, 0, sizeof(file));
    make_footer(footer, 0U, 16U, 0U, 0U, 0U, 0U, 0U);

    memcpy(footer, "GBAX", 4U);
    expect_result(&memory, ROMX_E_INVALID_FOOTER);
    memcpy(footer, "ROMX", 4U);

    write_le32(footer + 0x04U, 2U);
    expect_result(&memory, ROMX_E_INVALID_FOOTER);
    write_le32(footer + 0x04U, ROMX_FORMAT_VERSION_1);

    write_le32(footer + 0x5CU, 64U);
    expect_result(&memory, ROMX_E_INVALID_FOOTER);
}

static void test_flags(void)
{
    uint8_t file[32U + ROMX_FOOTER_SIZE_V1];
    memory_io_t memory = { file, sizeof(file), 0 };
    uint8_t *footer = file + 32U;

    memset(file, 0, sizeof(file));
    make_footer(footer, 0U, 16U, 16U, 8U, 0U, 0U, 0U);
    expect_result(&memory, ROMX_E_INVALID_FLAGS);

    write_le32(footer + 0x58U, ROMX_FLAG_HAS_METADATA | UINT32_C(0x80000000));
    expect_result(&memory, ROMX_E_INVALID_FLAGS);

    write_le32(footer + 0x58U, ROMX_FLAG_HAS_METADATA);
    footer[0x60U] = 1U;
    expect_result(&memory, ROMX_E_INVALID_FLAGS);
}

static void test_ranges_and_overlap(void)
{
    uint8_t file[64U + ROMX_FOOTER_SIZE_V1];
    memory_io_t memory = { file, sizeof(file), 0 };
    uint8_t *footer = file + 64U;

    memset(file, 0, sizeof(file));
    make_footer(footer, 0U, 0U, 0U, 0U, 0U, 0U, 0U);
    expect_result(&memory, ROMX_E_INVALID_FOOTER);

    make_footer(footer, UINT64_MAX - 3U, 8U, 0U, 0U, 0U, 0U, 0U);
    expect_result(&memory, ROMX_E_RANGE);

    make_footer(footer, 48U, 17U, 0U, 0U, 0U, 0U, 0U);
    expect_result(&memory, ROMX_E_RANGE);

    make_footer(footer, 0U, 32U, 16U, 20U, 0U, 0U,
        ROMX_FLAG_HAS_METADATA);
    expect_result(&memory, ROMX_E_OVERLAP);

    make_footer(footer, 0U, 16U, 16U, 16U, 32U, 32U,
        ROMX_FLAG_HAS_METADATA | ROMX_FLAG_HAS_COVER);
    expect_result(&memory, ROMX_OK);
}

static void test_truncation(void)
{
    uint8_t short_file[127U];
    uint8_t file[1U + ROMX_FOOTER_SIZE_V1];
    memory_io_t short_memory = { short_file, sizeof(short_file), 0 };
    memory_io_t partial_memory = { file, sizeof(file), 1 };

    memset(short_file, 0, sizeof(short_file));
    expect_result(&short_memory, ROMX_E_TRUNCATED);

    memset(file, 0, sizeof(file));
    make_footer(file + 1U, 0U, 1U, 0U, 0U, 0U, 0U, 0U);
    expect_result(&partial_memory, ROMX_E_TRUNCATED);
}

static void test_logical_file_larger_than_4_gib(void)
{
    virtual_io_t virtual_file;
    romx_io_t io = ROMX_IO_INIT;
    romx_reader_t *reader = NULL;
    romx_info_t info = ROMX_INFO_INIT;
    romx_error_t error;
    const uint64_t body_size = UINT64_C(0x100000000) + UINT64_C(4096);

    memset(&virtual_file, 0, sizeof(virtual_file));
    virtual_file.size = body_size + ROMX_FOOTER_SIZE_V1;
    make_footer(virtual_file.footer,
        UINT64_C(0), body_size,
        0U, 0U, 0U, 0U, 0U);
    io.user_data = &virtual_file;
    io.get_size = virtual_get_size;
    io.read_at = virtual_read_at;

    CHECK(romx_reader_open_io(&io, NULL, &reader, &error) == ROMX_OK);
    CHECK(romx_reader_get_info(reader, &info, &error) == ROMX_OK);
    CHECK(info.body_size == body_size);
    CHECK(info.rom.offset == UINT64_C(0));
    CHECK(info.rom.size == body_size);
    romx_reader_close(reader);
}

static void test_path_is_not_identity(void)
{
    uint8_t file[4U + ROMX_FOOTER_SIZE_V1];
    const char *path = "romx-phase1-valid.txt";
    FILE *output;
    romx_reader_t *reader = NULL;
    romx_error_t error;

    memset(file, 0, sizeof(file));
    make_footer(file + 4U, 0U, 4U, 0U, 0U, 0U, 0U, 0U);
    output = fopen(path, "wb");
    CHECK(output != NULL);
    if (output == NULL) {
        return;
    }
    CHECK(fwrite(file, 1U, sizeof(file), output) == sizeof(file));
    CHECK(fclose(output) == 0);

    CHECK(romx_reader_open_path(path, NULL, &reader, &error) == ROMX_OK);
    romx_reader_close(reader);
    CHECK(remove(path) == 0);
}

static void test_argument_validation(void)
{
    uint8_t file[1U + ROMX_FOOTER_SIZE_V1];
    memory_io_t memory = { file, sizeof(file), 0 };
    romx_error_t error;
    romx_info_t info = ROMX_INFO_INIT;
    romx_reader_t *reader = NULL;
    romx_io_t io = ROMX_IO_INIT;
    romx_reader_options_t options = ROMX_READER_OPTIONS_INIT;

    memset(file, 0, sizeof(file));
    make_footer(file + 1U, 0U, 1U, 0U, 0U, 0U, 0U, 0U);

    CHECK(romx_reader_open_path(NULL, NULL, &reader, &error) ==
        ROMX_E_INVALID_ARGUMENT);
    CHECK(romx_reader_open_io(&io, NULL, &reader, &error) ==
        ROMX_E_INVALID_ARGUMENT);
    CHECK(romx_reader_get_info(NULL, &info, &error) ==
        ROMX_E_INVALID_ARGUMENT);
    io.user_data = &memory;
    io.get_size = memory_get_size;
    io.read_at = memory_read_at;
    options.reserved = 1U;
    CHECK(romx_reader_open_io(&io, &options, &reader, &error) ==
        ROMX_E_INVALID_ARGUMENT);
    CHECK(strcmp(romx_result_string(ROMX_E_INVALID_FOOTER),
        "invalid ROMX footer") == 0);
}

int main(void)
{
    test_valid_canonical();
    test_valid_reordered_and_empty_offsets();
    test_identity_and_fixed_fields();
    test_flags();
    test_ranges_and_overlap();
    test_truncation();
    test_logical_file_larger_than_4_gib();
    test_path_is_not_identity();
    test_argument_validation();

    if (failures != 0) {
        fprintf(stderr, "%d phase 1 test(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("all phase 1 tests passed");
    return EXIT_SUCCESS;
}
