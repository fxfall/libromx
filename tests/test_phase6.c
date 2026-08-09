#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#endif

#include <romx/romx.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <unistd.h>
#endif

typedef struct memory_input {
    const uint8_t *bytes;
    uint64_t size;
    uint64_t maximum_read;
    int truncate;
} memory_input_t;

static int failures;

#define CHECK(value) do { \
    if (!(value)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value); \
        ++failures; \
    } \
} while (0)

static romx_result_t memory_size(void *user, uint64_t *size,
    romx_error_t *error)
{
    memory_input_t *input = (memory_input_t *)user;
    (void)error;
    *size = input->size;
    return ROMX_OK;
}

static romx_result_t memory_read(void *user, uint64_t offset, void *buffer,
    uint64_t size, uint64_t *bytes_read, romx_error_t *error)
{
    memory_input_t *input = (memory_input_t *)user;
    uint64_t count = size;
    (void)error;
    if (offset >= input->size || (input->truncate && offset >= 1U)) {
        *bytes_read = 0U;
        return ROMX_OK;
    }
    if (count > input->size - offset) count = input->size - offset;
    if (input->maximum_read != 0U && count > input->maximum_read)
        count = input->maximum_read;
    memcpy(buffer, input->bytes + (size_t)offset, (size_t)count);
    *bytes_read = count;
    return ROMX_OK;
}

static romx_io_t make_io(memory_input_t *input)
{
    romx_io_t io = ROMX_IO_INIT;
    io.user_data = input;
    io.get_size = memory_size;
    io.read_at = memory_read;
    return io;
}

static int payload_equals(const char *path, const char *expected)
{
    romx_reader_t *reader = NULL;
    romx_error_t error;
    uint8_t output[4] = {0};
    uint64_t count = 0U;
    int equal = 0;
    if (romx_reader_open_path(path, NULL, &reader, &error) == ROMX_OK &&
        romx_reader_read_region(reader, ROMX_REGION_ROM, 0U, output, 3U,
            &count, &error) == ROMX_OK) {
        equal = count == 3U && memcmp(output, expected, 3U) == 0;
    }
    romx_reader_close(reader);
    return equal;
}

static void test_streaming_writer(void)
{
#if defined(_WIN32)
    const char *path = "romx-phase6-output.romx";
    const char *bad_path = "romx-phase6-bad.romx";
#else
    char directory[] = "/tmp/libromx-phase6-XXXXXX";
    char path[512];
    char bad_path[512];
    CHECK(mkdtemp(directory) != NULL);
    (void)snprintf(path, sizeof(path), "%s/output.romx", directory);
    (void)snprintf(bad_path, sizeof(bad_path), "%s/bad.romx", directory);
#endif
    {
        static const uint8_t abc[] = { 'a', 'b', 'c' };
        static const uint8_t abd[] = { 'a', 'b', 'd' };
        memory_input_t input = { abc, 3U, 1U, 0 };
        memory_input_t replacement = { abd, 3U, 2U, 0 };
        memory_input_t truncated = { abc, 3U, 1U, 1 };
        romx_io_t io = make_io(&input);
        romx_io_t replacement_io = make_io(&replacement);
        romx_io_t truncated_io = make_io(&truncated);
        romx_writer_report_t report = ROMX_WRITER_REPORT_INIT;
        romx_writer_options_t options = ROMX_WRITER_OPTIONS_INIT;
        romx_reader_t *reader = NULL;
        romx_validation_report_t validation = ROMX_VALIDATION_REPORT_INIT;
        romx_info_t info = ROMX_INFO_INIT;
        romx_error_t error;
        size_t index;

        CHECK(romx_writer_write_io_path(path, &io, NULL, 0U, NULL,
            NULL, &report, &error) == ROMX_OK);
        CHECK(report.payload_size == 3U);
        CHECK(report.payload_crc32 == UINT32_C(0x352441c2));
        CHECK(report.flags == 0U);
        CHECK(payload_equals(path, "abc"));
        CHECK(romx_reader_open_path(path, NULL, &reader, &error) == ROMX_OK);
        CHECK(romx_reader_get_info(reader, &info, &error) == ROMX_OK);
        for (index = 0U; index < sizeof(info.reserved); ++index)
            CHECK(info.reserved[index] == 0U);
        CHECK(romx_reader_validate(reader, ROMX_VALIDATE_ALL,
            &validation, &error) == ROMX_OK);
        CHECK(validation.body_sha256 == ROMX_STATUS_ABSENT);
        romx_reader_close(reader);

        CHECK(romx_writer_write_io_path(path, &io, NULL, 0U, NULL,
            NULL, &report, &error) == ROMX_E_EXISTS);
        options.flags = ROMX_WRITER_REPLACE_EXISTING |
            ROMX_WRITER_BODY_SHA256 | ROMX_WRITER_DURABLE;
        report = (romx_writer_report_t)ROMX_WRITER_REPORT_INIT;
        CHECK(romx_writer_write_io_path(path, &replacement_io, NULL, 0U,
            NULL, &options, &report, &error) == ROMX_OK);
        CHECK((report.flags & ROMX_FLAG_HAS_BODY_SHA256) != 0U);
        CHECK(payload_equals(path, "abd"));
        reader = NULL;
        validation = (romx_validation_report_t)ROMX_VALIDATION_REPORT_INIT;
        CHECK(romx_reader_open_path(path, NULL, &reader, &error) == ROMX_OK);
        CHECK(romx_reader_validate(reader, ROMX_VALIDATE_ALL,
            &validation, &error) == ROMX_OK);
        CHECK(validation.body_sha256 == ROMX_STATUS_VALID);
        romx_reader_close(reader);

        CHECK(romx_writer_write_io_path(bad_path, &truncated_io,
            NULL, 0U, NULL, NULL, &report, &error) == ROMX_E_TRUNCATED);
        {
            FILE *bad = fopen(bad_path, "rb");
            CHECK(bad == NULL);
            if (bad != NULL) fclose(bad);
        }
    }
#if defined(_WIN32)
    (void)remove(path);
    (void)remove(bad_path);
#else
    CHECK(unlink(path) == 0);
    (void)unlink(bad_path);
    CHECK(rmdir(directory) == 0);
#endif
}

int main(void)
{
    test_streaming_writer();
    if (failures != 0) return EXIT_FAILURE;
    puts("all phase 6 tests passed");
    return EXIT_SUCCESS;
}
