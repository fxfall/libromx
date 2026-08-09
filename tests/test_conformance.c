#include <romx/romx.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct fixture_case {
    const char *name;
    romx_result_t open_result;
    romx_result_t validate_result;
    romx_validation_status_t body_status;
    romx_validation_status_t metadata_status;
    romx_crc32_status_t crc32_status;
    romx_validation_status_t cover_status;
    romx_result_t metadata_result;
    romx_result_t cover_result;
    int extract_payload;
} fixture_case_t;

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static const fixture_case_t cases[] = {
    { "body-sha256-disabled-nonzero", ROMX_E_INVALID_FLAGS, 0, 0, 0, 0, 0, 0, 0, 0 },
    { "body-sha256-mismatch", ROMX_OK, ROMX_E_BODY_HASH, ROMX_STATUS_INVALID, ROMX_STATUS_ABSENT, ROMX_CRC32_ABSENT, ROMX_STATUS_ABSENT, ROMX_E_METADATA_ABSENT, ROMX_E_COVER_ABSENT, 0 },
    { "cover-absent", ROMX_OK, ROMX_OK, ROMX_STATUS_VALID, ROMX_STATUS_VALID, ROMX_CRC32_VALID_LOOKUP, ROMX_STATUS_ABSENT, ROMX_OK, ROMX_E_COVER_ABSENT, 1 },
    { "cover-absent-nonzero-offset", ROMX_OK, ROMX_OK, ROMX_STATUS_VALID, ROMX_STATUS_VALID, ROMX_CRC32_VALID_LOOKUP, ROMX_STATUS_ABSENT, ROMX_OK, ROMX_E_COVER_ABSENT, 1 },
    { "cover-chunk-crc-mismatch", ROMX_OK, ROMX_OK, ROMX_STATUS_VALID, ROMX_STATUS_VALID, ROMX_CRC32_VALID_LOOKUP, ROMX_STATUS_INVALID, ROMX_OK, ROMX_E_COVER_PNG, 1 },
    { "cover-chunk-out-of-bounds", ROMX_OK, ROMX_OK, ROMX_STATUS_VALID, ROMX_STATUS_VALID, ROMX_CRC32_VALID_LOOKUP, ROMX_STATUS_INVALID, ROMX_OK, ROMX_E_COVER_PNG, 1 },
    { "cover-missing-iend", ROMX_OK, ROMX_OK, ROMX_STATUS_VALID, ROMX_STATUS_VALID, ROMX_CRC32_VALID_LOOKUP, ROMX_STATUS_INVALID, ROMX_OK, ROMX_E_COVER_PNG, 1 },
    { "flags-mismatch", ROMX_E_INVALID_FLAGS, 0, 0, 0, 0, 0, 0, 0, 0 },
    { "footer-magic-invalid", ROMX_E_INVALID_FOOTER, 0, 0, 0, 0, 0, 0, 0, 0 },
    { "footer-size-invalid", ROMX_E_INVALID_FOOTER, 0, 0, 0, 0, 0, 0, 0, 0 },
    { "footer-version-invalid", ROMX_E_INVALID_FOOTER, 0, 0, 0, 0, 0, 0, 0, 0 },
    { "metadata-absent", ROMX_OK, ROMX_OK, ROMX_STATUS_VALID, ROMX_STATUS_ABSENT, ROMX_CRC32_ABSENT, ROMX_STATUS_ABSENT, ROMX_E_METADATA_ABSENT, ROMX_E_COVER_ABSENT, 1 },
    { "metadata-absent-nonzero-offset", ROMX_OK, ROMX_OK, ROMX_STATUS_VALID, ROMX_STATUS_ABSENT, ROMX_CRC32_ABSENT, ROMX_STATUS_ABSENT, ROMX_E_METADATA_ABSENT, ROMX_E_COVER_ABSENT, 1 },
    { "metadata-bom", ROMX_OK, ROMX_OK, ROMX_STATUS_VALID, ROMX_STATUS_INVALID, ROMX_CRC32_INVALID, ROMX_STATUS_ABSENT, ROMX_E_METADATA_UTF8, ROMX_E_COVER_ABSENT, 1 },
    { "metadata-crc32-lookup", ROMX_OK, ROMX_OK, ROMX_STATUS_VALID, ROMX_STATUS_VALID, ROMX_CRC32_VALID_LOOKUP, ROMX_STATUS_ABSENT, ROMX_OK, ROMX_E_COVER_ABSENT, 1 },
    { "metadata-duplicate-key", ROMX_OK, ROMX_OK, ROMX_STATUS_VALID, ROMX_STATUS_INVALID, ROMX_CRC32_INVALID, ROMX_STATUS_ABSENT, ROMX_E_METADATA_SCHEMA, ROMX_E_COVER_ABSENT, 1 },
    { "metadata-nested-duplicate-key", ROMX_OK, ROMX_OK, ROMX_STATUS_VALID, ROMX_STATUS_INVALID, ROMX_CRC32_INVALID, ROMX_STATUS_ABSENT, ROMX_E_METADATA_SCHEMA, ROMX_E_COVER_ABSENT, 1 },
    { "metadata-invalid-utf8", ROMX_OK, ROMX_OK, ROMX_STATUS_VALID, ROMX_STATUS_INVALID, ROMX_CRC32_INVALID, ROMX_STATUS_ABSENT, ROMX_E_METADATA_UTF8, ROMX_E_COVER_ABSENT, 1 },
    { "minimal-valid", ROMX_OK, ROMX_OK, ROMX_STATUS_ABSENT, ROMX_STATUS_ABSENT, ROMX_CRC32_ABSENT, ROMX_STATUS_ABSENT, ROMX_E_METADATA_ABSENT, ROMX_E_COVER_ABSENT, 1 },
    { "offset-out-of-bounds", ROMX_E_RANGE, 0, 0, 0, 0, 0, 0, 0, 0 },
    { "offset-overflow", ROMX_E_RANGE, 0, 0, 0, 0, 0, 0, 0, 0 },
    { "payload-salvage", ROMX_OK, ROMX_OK, ROMX_STATUS_VALID, ROMX_STATUS_INVALID, ROMX_CRC32_INVALID, ROMX_STATUS_INVALID, ROMX_E_METADATA_UTF8, ROMX_E_COVER_PNG, 1 },
    { "regions-overlap", ROMX_E_OVERLAP, 0, 0, 0, 0, 0, 0, 0, 0 },
    { "regions-gap", ROMX_E_RANGE, 0, 0, 0, 0, 0, 0, 0, 0 },
    { "reordered-regions", ROMX_OK, ROMX_OK, ROMX_STATUS_VALID, ROMX_STATUS_VALID, ROMX_CRC32_VALID_LOOKUP, ROMX_STATUS_VALID, ROMX_OK, ROMX_OK, 1 },
    { "reserved-nonzero", ROMX_OK, ROMX_OK, ROMX_STATUS_VALID, ROMX_STATUS_ABSENT, ROMX_CRC32_ABSENT, ROMX_STATUS_ABSENT, ROMX_E_METADATA_ABSENT, ROMX_E_COVER_ABSENT, 1 }
};

static int file_is_abc(const char *path)
{
    FILE *file = fopen(path, "rb");
    uint8_t bytes[4];
    size_t size;

    if (file == NULL) {
        return 0;
    }
    size = fread(bytes, 1U, sizeof(bytes), file);
    if (fclose(file) != 0) {
        return 0;
    }
    return size == 3U && memcmp(bytes, "abc", 3U) == 0;
}

int main(int argc, char **argv)
{
    size_t index;
    const char *output = "romx-conformance-payload.tmp";

    if (argc != 2) {
        fprintf(stderr, "usage: %s FIXTURE_DIRECTORY\n", argv[0]);
        return EXIT_FAILURE;
    }

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        const fixture_case_t *item = &cases[index];
        char path[2048];
        romx_reader_t *reader = NULL;
        romx_validation_report_t report = ROMX_VALIDATION_REPORT_INIT;
        romx_error_t error;
        romx_result_t result;

        (void)snprintf(path, sizeof(path), "%s/%s.romx", argv[1], item->name);
        result = romx_reader_open_path(path, NULL, &reader, &error);
        if (result != item->open_result) {
            fprintf(stderr, "%s: open returned %s: %s\n", item->name,
                romx_result_string(result), error.message);
            ++failures;
            romx_reader_close(reader);
            continue;
        }
        if (result != ROMX_OK) {
            continue;
        }

        result = romx_reader_validate(reader, ROMX_VALIDATE_ALL,
            &report, &error);
        if (result != item->validate_result) {
            fprintf(stderr, "%s: validate returned %s: %s\n", item->name,
                romx_result_string(result), error.message);
            ++failures;
        }
        CHECK(report.structure == ROMX_STATUS_VALID);
        CHECK(report.payload_hashes == ROMX_STATUS_VALID);
        CHECK(report.computed_payload_crc32 == UINT32_C(0x352441c2));
        CHECK(report.body_sha256 == item->body_status);
        CHECK(report.metadata == item->metadata_status);
        CHECK(report.metadata_crc32 == item->crc32_status);
        CHECK(report.cover == item->cover_status);
        CHECK(report.metadata_result == item->metadata_result);
        CHECK(report.cover_result == item->cover_result);

        if (strcmp(item->name, "reserved-nonzero") == 0) {
            romx_info_t info = ROMX_INFO_INIT;
            CHECK(romx_reader_get_info(reader, &info, &error) == ROMX_OK);
            CHECK(info.reserved[0] == UINT8_C(0xa5));
        }
        if (strcmp(item->name, "metadata-absent-nonzero-offset") == 0) {
            romx_info_t info = ROMX_INFO_INIT;
            CHECK(romx_reader_get_info(reader, &info, &error) == ROMX_OK);
            CHECK(info.metadata.size == UINT64_C(0));
            CHECK(info.metadata.offset == UINT64_C(0));
        }
        if (strcmp(item->name, "cover-absent-nonzero-offset") == 0) {
            romx_info_t info = ROMX_INFO_INIT;
            CHECK(romx_reader_get_info(reader, &info, &error) == ROMX_OK);
            CHECK(info.cover.size == UINT64_C(0));
            CHECK(info.cover.offset == UINT64_C(0));
        }

        if (item->extract_payload) {
            romx_extract_options_t options = ROMX_EXTRACT_OPTIONS_INIT;
            options.flags = ROMX_EXTRACT_REPLACE_EXISTING;
            result = romx_extract_payload_path(reader, output, &options, &error);
            if (result != ROMX_OK || !file_is_abc(output)) {
                fprintf(stderr, "%s: payload extraction failed: %s\n",
                    item->name, error.message);
                ++failures;
            }
            (void)remove(output);
        }
        romx_reader_close(reader);
    }

    if (failures != 0) {
        fprintf(stderr, "%d conformance check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    puts("all frozen ROMX conformance fixtures passed");
    return EXIT_SUCCESS;
}
