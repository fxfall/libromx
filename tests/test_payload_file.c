#include <romx/romx.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", \
            __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static void le32(uint8_t *p, uint32_t value)
{
    unsigned int index;
    for (index = 0U; index < 4U; ++index) {
        p[index] = (uint8_t)(value >> (index * 8U));
    }
}

static void le64(uint8_t *p, uint64_t value)
{
    unsigned int index;
    for (index = 0U; index < 8U; ++index) {
        p[index] = (uint8_t)(value >> (index * 8U));
    }
}

#if !defined(_WIN32)
static int write_all(int descriptor, const uint8_t *data, size_t size)
{
    size_t written = 0U;
    while (written < size) {
        ssize_t actual = write(descriptor, data + written, size - written);
        if (actual <= 0) return 0;
        written += (size_t)actual;
    }
    return 1;
}

static void test_payload_file(void)
{
    static const char metadata[] =
        "{\"schema_version\":\"0.1.0\",\"name\":\"Payload file test\","
        "\"platform\":\"gba\",\"payload_format\":\"gba\","
        "\"crc32\":\"1234abcd\"}";
    static const uint8_t payload[] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
    };
    char path[] = "/tmp/libromx-payload-file-XXXXXX";
    uint8_t footer[ROMX_FOOTER_SIZE_0_1_0];
    int descriptor;
    romx_payload_file_t *file = NULL;
    romx_payload_file_t *second_file = NULL;
    romx_payload_file_options_t options = ROMX_PAYLOAD_FILE_OPTIONS_INIT;
    romx_reader_t *reader = NULL;
    romx_metadata_t *metadata_handle = NULL;
    romx_error_t error;
    uint64_t size = 0U;
    uint64_t position = 0U;
    uint64_t bytes_read = 0U;
    uint32_t crc32 = 0U;
    uint8_t output[8] = { 0 };

    memset(footer, 0, sizeof(footer));
    memcpy(footer, "ROMX", 4U);
    le32(footer + 0x04U, ROMX_FORMAT_VERSION_0_1_0);
    le64(footer + 0x08U, sizeof(metadata) - 1U);
    le64(footer + 0x10U, sizeof(payload));
    le64(footer + 0x20U, sizeof(metadata) - 1U);
    le32(footer + 0x58U, ROMX_FLAG_HAS_METADATA);
    le32(footer + 0x5cU, ROMX_FOOTER_SIZE_0_1_0);

    descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    if (descriptor < 0) return;
    CHECK(write_all(descriptor, (const uint8_t *)metadata,
        sizeof(metadata) - 1U));
    CHECK(write_all(descriptor, payload, sizeof(payload)));
    CHECK(write_all(descriptor, footer, sizeof(footer)));
    CHECK(close(descriptor) == 0);

    CHECK(romx_payload_file_open_path(path, NULL, &options, &file, &error) == ROMX_OK);
    CHECK(romx_payload_file_open_path(path, NULL, &options,
        &second_file, &error) == ROMX_OK);
    CHECK(romx_payload_file_get_size(file, &size, &error) == ROMX_OK);
    CHECK(size == sizeof(payload));
    CHECK(romx_payload_file_tell(file, &position, &error) == ROMX_OK);
    CHECK(position == 0U);
    CHECK(romx_payload_file_read(second_file, output, 1U,
        &bytes_read, &error) == ROMX_OK);
    CHECK(bytes_read == 1U && output[0] == payload[0]);
    CHECK(romx_payload_file_tell(file, &position, &error) == ROMX_OK);
    CHECK(position == 0U);
    CHECK(romx_payload_file_read(file, output, 4U, &bytes_read, &error) == ROMX_OK);
    CHECK(bytes_read == 4U && memcmp(output, payload, 4U) == 0);
    CHECK(romx_payload_file_seek(file, -2, ROMX_PAYLOAD_SEEK_CURRENT,
        &position, &error) == ROMX_OK);
    CHECK(position == 2U);
    CHECK(romx_payload_file_read(file, output, sizeof(output),
        &bytes_read, &error) == ROMX_OK);
    CHECK(bytes_read == sizeof(output));
    CHECK(memcmp(output, payload + 2U, sizeof(output)) == 0);
    CHECK(romx_payload_file_seek(file, -1, ROMX_PAYLOAD_SEEK_END,
        &position, &error) == ROMX_OK);
    CHECK(position == sizeof(payload) - 1U);
    CHECK(romx_payload_file_read(file, output, 2U, &bytes_read, &error) == ROMX_OK);
    CHECK(bytes_read == 1U && output[0] == payload[sizeof(payload) - 1U]);
    CHECK(romx_payload_file_seek(file, 10, ROMX_PAYLOAD_SEEK_END,
        &position, &error) == ROMX_OK);
    CHECK(romx_payload_file_read(file, output, 1U, &bytes_read, &error) == ROMX_OK);
    CHECK(bytes_read == 0U);
    CHECK(romx_payload_file_seek(file, -1, ROMX_PAYLOAD_SEEK_START,
        &position, &error) == ROMX_E_RANGE);
    romx_payload_file_close(second_file);
    romx_payload_file_close(file);

    CHECK(romx_reader_open_path(path, NULL, &reader, &error) == ROMX_OK);
    CHECK(romx_metadata_open(reader, &metadata_handle, &error) == ROMX_OK);
    CHECK(romx_metadata_get_crc32(metadata_handle, &crc32, &error) == ROMX_OK);
    CHECK(crc32 == UINT32_C(0x1234abcd));
    romx_metadata_close(metadata_handle);
    romx_reader_close(reader);
    CHECK(remove(path) == 0);
}
#endif

int main(void)
{
#if !defined(_WIN32)
    test_payload_file();
#endif
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
