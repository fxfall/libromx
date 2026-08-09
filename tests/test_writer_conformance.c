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
} memory_input_t;

typedef struct writer_case {
    const char *name;
    const char *metadata;
    const uint8_t *cover;
    uint64_t cover_size;
    const char *lookup_crc32;
    romx_writer_flags_t flags;
} writer_case_t;

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static const uint8_t png[] = {
    0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,
    0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,
    0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
    0x08,0x06,0x00,0x00,0x00,0x1f,0x15,0xc4,
    0x89,0x00,0x00,0x00,0x0b,0x49,0x44,0x41,
    0x54,0x78,0x9c,0x63,0x60,0x00,0x02,0x00,
    0x00,0x05,0x00,0x01,0x7a,0x5e,0xab,0x3f,
    0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,
    0xae,0x42,0x60,0x82
};

static romx_result_t memory_get_size(void *user, uint64_t *size,
    romx_error_t *error)
{
    memory_input_t *input = (memory_input_t *)user;
    (void)error;
    *size = input->size;
    return ROMX_OK;
}

static romx_result_t memory_read_at(void *user, uint64_t offset,
    void *buffer, uint64_t size, uint64_t *bytes_read, romx_error_t *error)
{
    memory_input_t *input = (memory_input_t *)user;
    (void)error;
    if (offset > input->size || size > input->size - offset) {
        *bytes_read = UINT64_C(0);
        return ROMX_OK;
    }
    memcpy(buffer, input->bytes + (size_t)offset, (size_t)size);
    *bytes_read = size;
    return ROMX_OK;
}

static romx_io_t memory_io(memory_input_t *input)
{
    romx_io_t io = ROMX_IO_INIT;
    io.user_data = input;
    io.get_size = memory_get_size;
    io.read_at = memory_read_at;
    return io;
}

static int files_equal(const char *first_path, const char *second_path)
{
    FILE *first = fopen(first_path, "rb");
    FILE *second = fopen(second_path, "rb");
    uint8_t first_bytes[4096];
    uint8_t second_bytes[4096];
    int equal = first != NULL && second != NULL;

    while (equal) {
        size_t first_size = fread(first_bytes, 1U, sizeof(first_bytes), first);
        size_t second_size = fread(second_bytes, 1U, sizeof(second_bytes), second);
        if (first_size != second_size ||
            memcmp(first_bytes, second_bytes, first_size) != 0) {
            equal = 0;
        }
        if (first_size < sizeof(first_bytes)) {
            if (ferror(first) || ferror(second)) equal = 0;
            break;
        }
    }
    if (first != NULL && fclose(first) != 0) equal = 0;
    if (second != NULL && fclose(second) != 0) equal = 0;
    return equal;
}

int main(int argc, char **argv)
{
    static const uint8_t payload_bytes[] = { 'a', 'b', 'c' };
    static const char writer_metadata[] =
        "{\"schema_version\":\"0.1.0\",\"name\":\"ROMX writer golden\","
        "\"platform\":\"gb\",\"payload_format\":\"gb\"}";
    static const char origin_metadata[] =
        "{\"schema_version\":\"0.1.0\",\"name\":\"ROMX writer golden\","
        "\"platform\":\"gb\",\"payload_format\":\"gb\","
        "\"origin_crc32\":\"00000000\"}";
    static const char body_metadata[] =
        "{\"schema_version\":\"0.1.0\","
        "\"name\":\"ROMX body SHA-256 golden\",\"platform\":\"gb\","
        "\"payload_format\":\"gb\"}";
    static const writer_case_t cases[] = {
        { "payload-only", NULL, NULL, UINT64_C(0), NULL, UINT32_C(0) },
        { "metadata-auto-crc32", writer_metadata, NULL, UINT64_C(0),
          NULL, UINT32_C(0) },
        { "metadata-lookup-crc32-override", writer_metadata, NULL,
          UINT64_C(0), "00000000", UINT32_C(0) },
        { "metadata-origin-crc32", origin_metadata, NULL, UINT64_C(0),
          NULL, UINT32_C(0) },
        { "cover", writer_metadata, png, sizeof(png), NULL, UINT32_C(0) },
        { "body-sha256-disabled", body_metadata, NULL, UINT64_C(0),
          NULL, UINT32_C(0) },
        { "body-sha256-enabled", body_metadata, NULL, UINT64_C(0), NULL,
          ROMX_WRITER_BODY_SHA256 }
    };
    memory_input_t payload_input = { payload_bytes, sizeof(payload_bytes) };
    romx_io_t payload = memory_io(&payload_input);
    size_t index;
#if defined(_WIN32)
    const char *directory = ".";
#else
    char directory[] = "/tmp/libromx-writer-conformance-XXXXXX";
#endif

    if (argc != 2) {
        fprintf(stderr, "usage: %s FIXTURE_DIRECTORY\n", argv[0]);
        return EXIT_FAILURE;
    }
#if !defined(_WIN32)
    CHECK(mkdtemp(directory) != NULL);
#endif
    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        const writer_case_t *item = &cases[index];
        memory_input_t cover_input = { item->cover, item->cover_size };
        romx_io_t cover = memory_io(&cover_input);
        romx_writer_options_t options = ROMX_WRITER_OPTIONS_INIT;
        romx_writer_report_t report = ROMX_WRITER_REPORT_INIT;
        romx_error_t error;
        char output_path[2048];
        char fixture_path[2048];
        const void *metadata = item->metadata;
        uint64_t metadata_size = item->metadata != NULL
            ? (uint64_t)strlen(item->metadata) : UINT64_C(0);

        options.flags = item->flags;
        options.lookup_crc32 = item->lookup_crc32;
        (void)snprintf(output_path, sizeof(output_path), "%s/%s.romx",
            directory, item->name);
        (void)snprintf(fixture_path, sizeof(fixture_path),
            "%s/writer/%s.romx", argv[1], item->name);
        if (romx_writer_write_io_path(output_path, &payload, metadata,
                metadata_size, item->cover != NULL ? &cover : NULL,
                &options, &report, &error) != ROMX_OK) {
            fprintf(stderr, "%s: writer failed: %s\n", item->name,
                error.message);
            ++failures;
        } else if (!files_equal(output_path, fixture_path)) {
            fprintf(stderr, "%s: output differs from frozen golden\n",
                item->name);
            ++failures;
        }
        (void)remove(output_path);
    }
#if !defined(_WIN32)
    CHECK(rmdir(directory) == 0);
#endif
    if (failures != 0) return EXIT_FAILURE;
    puts("all frozen ROMX writer golden fixtures passed");
    return EXIT_SUCCESS;
}
