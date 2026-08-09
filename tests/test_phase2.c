#include <romx/romx.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct memory_io {
    const uint8_t *data;
    uint64_t size;
} memory_io_t;

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static void le32(uint8_t *p, uint32_t v)
{
    unsigned int i;
    for (i = 0U; i < 4U; ++i) p[i] = (uint8_t)(v >> (i * 8U));
}

static void le64(uint8_t *p, uint64_t v)
{
    unsigned int i;
    for (i = 0U; i < 8U; ++i) p[i] = (uint8_t)(v >> (i * 8U));
}

static romx_result_t get_size(void *user, uint64_t *size, romx_error_t *error)
{
    (void)error;
    *size = ((memory_io_t *)user)->size;
    return ROMX_OK;
}

static romx_result_t read_at(void *user, uint64_t offset, void *buffer,
    uint64_t size, uint64_t *read, romx_error_t *error)
{
    memory_io_t *memory = (memory_io_t *)user;
    (void)error;
    if (offset > memory->size || size > memory->size - offset) {
        *read = 0U;
        return ROMX_OK;
    }
    memcpy(buffer, memory->data + (size_t)offset, (size_t)size);
    *read = size;
    return ROMX_OK;
}

static romx_reader_t *open_file(uint8_t *file, uint64_t size)
{
    static memory_io_t memory;
    romx_io_t io = ROMX_IO_INIT;
    romx_reader_t *reader = NULL;
    romx_error_t error;
    memory.data = file;
    memory.size = size;
    io.user_data = &memory;
    io.get_size = get_size;
    io.read_at = read_at;
    CHECK(romx_reader_open_io(&io, NULL, &reader, &error) == ROMX_OK);
    return reader;
}

static void make_abc_file(uint8_t file[3U + ROMX_FOOTER_SIZE_0_1_0])
{
    static const uint8_t sha256_abc[32] = {
        0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,
        0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,
        0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad
    };
    uint8_t *footer = file + 3U;
    memcpy(file, "abc", 3U);
    memset(footer, 0, ROMX_FOOTER_SIZE_0_1_0);
    memcpy(footer, "ROMX", 4U);
    le32(footer + 4U, 1U);
    le64(footer + 8U, 0U);
    le64(footer + 16U, 3U);
    memset(footer + 0x38U, 0xa5, 32U);
    le32(footer + 0x58U, ROMX_FLAG_HAS_BODY_SHA256);
    le32(footer + 0x5cU, ROMX_FOOTER_SIZE_0_1_0);
    memcpy(footer + 0x60U, sha256_abc, 32U);
}

static void test_known_hashes(void)
{
    uint8_t file[3U + ROMX_FOOTER_SIZE_0_1_0];
    romx_reader_t *reader;
    romx_validation_report_t report = ROMX_VALIDATION_REPORT_INIT;
    romx_error_t error;
    make_abc_file(file);
    reader = open_file(file, sizeof(file));
    CHECK(romx_reader_validate(reader,
        ROMX_VALIDATE_PAYLOAD_HASHES | ROMX_VALIDATE_BODY_SHA256,
        &report, &error) == ROMX_OK);
    CHECK(report.payload_hashes == ROMX_STATUS_VALID);
    CHECK(report.body_sha256 == ROMX_STATUS_VALID);
    CHECK(report.computed_payload_crc32 == UINT32_C(0x352441c2));
    romx_reader_close(reader);
}

static void test_hash_mismatch_and_region_read(void)
{
    uint8_t file[3U + ROMX_FOOTER_SIZE_0_1_0];
    uint8_t output[4] = {0};
    uint64_t read = 0U;
    romx_reader_t *reader;
    romx_validation_report_t report = ROMX_VALIDATION_REPORT_INIT;
    romx_error_t error;
    make_abc_file(file);
    file[1] = (uint8_t)'x';
    reader = open_file(file, sizeof(file));
    CHECK(romx_reader_validate(reader, ROMX_VALIDATE_PAYLOAD_HASHES |
        ROMX_VALIDATE_BODY_SHA256, &report, &error) == ROMX_E_BODY_HASH);
    CHECK(report.payload_hashes == ROMX_STATUS_VALID);
    CHECK(report.body_sha256 == ROMX_STATUS_INVALID);
    CHECK(romx_reader_read_region(reader, ROMX_REGION_ROM, 1U,
        output, 2U, &read, &error) == ROMX_OK);
    CHECK(read == 2U && output[0] == (uint8_t)'x' && output[1] == (uint8_t)'c');
    CHECK(romx_reader_read_region(reader, ROMX_REGION_ROM, 4U,
        output, 1U, &read, &error) == ROMX_E_RANGE);
    romx_reader_close(reader);
}

int main(void)
{
    test_known_hashes();
    test_hash_mismatch_and_region_read();
    if (failures != 0) return EXIT_FAILURE;
    puts("all phase 2 tests passed");
    return EXIT_SUCCESS;
}
