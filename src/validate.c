#include "romx_internal.h"

#include <string.h>

romx_result_t romx_reader_validate(
    const romx_reader_t *reader,
    romx_validate_flags_t flags,
    romx_validation_report_t *report,
    romx_error_t *error)
{
    uint32_t supplied_size;
    romx_result_t result = ROMX_OK;
    romx_error_t optional_error;

    romx_error_clear(error);
    if (reader == NULL || report == NULL) {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN,
            "reader and validation report must not be null");
    }
    if ((flags & ~ROMX_VALIDATE_ALL) != UINT32_C(0)) {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN, "unknown validation flags");
    }
    supplied_size = report->struct_size;
    if (supplied_size < sizeof(*report)) {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN, "validation report structure is too small");
    }

    memset(report, 0, sizeof(*report));
    report->struct_size = supplied_size;
    report->structure = ROMX_STATUS_VALID;

    if ((flags & ROMX_VALIDATE_PAYLOAD_HASHES) != 0U) {
        result = romx_hash_region(reader, reader->info.rom,
            report->computed_payload_sha256,
            &report->computed_payload_crc32, error);
        if (result != ROMX_OK) {
            return result;
        }
        /* These are derived values. ROMX 0.1.0 has no payload hash in its footer. */
        report->payload_hashes = ROMX_STATUS_VALID;
    }

    if ((flags & ROMX_VALIDATE_BODY_SHA256) != 0U) {
        if ((reader->info.flags & ROMX_FLAG_HAS_BODY_SHA256) == 0U) {
            report->body_sha256 = ROMX_STATUS_ABSENT;
        } else {
            romx_region_info_t body;
            body.offset = UINT64_C(0);
            body.size = reader->info.body_size;
            result = romx_hash_region(reader, body,
                report->computed_body_sha256, NULL, error);
            if (result != ROMX_OK) {
                return result;
            }
            report->body_sha256 = memcmp(report->computed_body_sha256,
                reader->info.body_sha256, 32U) == 0
                ? ROMX_STATUS_VALID
                : ROMX_STATUS_INVALID;
        }
    }

    if ((flags & ROMX_VALIDATE_METADATA) != 0U) {
        romx_error_clear(&optional_error);
        result = romx_validate_metadata_internal(
            reader, report, &optional_error);
        report->metadata_result = result;
        if (result != ROMX_OK && result != ROMX_E_METADATA_ABSENT &&
            result != ROMX_E_METADATA_TOO_LARGE &&
            result != ROMX_E_METADATA_UTF8 &&
            result != ROMX_E_METADATA_JSON &&
            result != ROMX_E_METADATA_SCHEMA) {
            if (error != NULL) {
                *error = optional_error;
            }
            return result;
        }
    }

    if ((flags & ROMX_VALIDATE_COVER) != 0U) {
        romx_error_clear(&optional_error);
        result = romx_validate_cover_internal(reader, report, &optional_error);
        report->cover_result = result;
        if (result != ROMX_OK && result != ROMX_E_COVER_ABSENT &&
            result != ROMX_E_COVER_TOO_LARGE &&
            result != ROMX_E_COVER_PNG) {
            if (error != NULL) {
                *error = optional_error;
            }
            return result;
        }
    }

    if (report->body_sha256 == ROMX_STATUS_INVALID) {
        return romx_error_set(error, ROMX_E_BODY_HASH, 0, UINT64_C(0),
            "container body SHA-256 does not match footer");
    }

    romx_error_clear(error);
    return ROMX_OK;
}

romx_result_t romx_validate_required_integrity(
    const romx_reader_t *reader,
    romx_error_t *error)
{
    romx_validation_report_t report = ROMX_VALIDATION_REPORT_INIT;

    return romx_reader_validate(reader, ROMX_VALIDATE_BODY_SHA256,
        &report, error);
}
