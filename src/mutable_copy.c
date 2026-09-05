#if !defined(_WIN32)
#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#endif

#include "romx_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#else
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/* This API intentionally uses the reader's region-copy primitive.  The
 * destination is only a file sink; no mutable header or object table is
 * decoded here, so unknown namespaces remain opaque and are preserved. */
typedef struct mutable_copy_sink {
    FILE *destination;
    uint64_t expected_size;
    uint64_t bytes_written;
} mutable_copy_sink_t;

#if defined(_WIN32)
static wchar_t *path_to_wide(const char *path)
{
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        path, -1, NULL, 0);
    wchar_t *wide;
    if (count <= 0) return NULL;
    wide = (wchar_t *)malloc((size_t)count * sizeof(*wide));
    if (wide == NULL) return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            path, -1, wide, count) <= 0) {
        free(wide);
        return NULL;
    }
    return wide;
}

static FILE *open_destination(const char *path)
{
    wchar_t *wide = path_to_wide(path);
    FILE *file = NULL;
    if (wide != NULL) (void)_wfopen_s(&file, wide, L"r+b");
    free(wide);
    return file;
}
#else
static FILE *open_destination(const char *path)
{
    return fopen(path, "r+b");
}
#endif

static int seek_destination(FILE *file, uint64_t offset)
{
#if defined(_WIN32)
    if (offset > (uint64_t)INT64_MAX) return -1;
    return _fseeki64(file, (__int64)offset, SEEK_SET);
#else
    if (offset > (uint64_t)INT64_MAX) return -1;
    return fseeko(file, (off_t)offset, SEEK_SET);
#endif
}

static romx_result_t copy_sink_write(void *user_data, const void *data,
    uint64_t size, romx_error_t *error)
{
    mutable_copy_sink_t *sink = (mutable_copy_sink_t *)user_data;
    size_t requested;
    size_t written = 0U;

    if (sink == NULL || sink->destination == NULL ||
        (data == NULL && size != UINT64_C(0)) ||
        sink->bytes_written > sink->expected_size ||
        size > sink->expected_size - sink->bytes_written ||
        size > (uint64_t)SIZE_MAX) {
        return romx_error_set(error, ROMX_E_RANGE, 0,
            sink == NULL ? ROMX_OFFSET_UNKNOWN : sink->bytes_written,
            "mutable copy exceeds destination region");
    }
    requested = (size_t)size;
    while (written < requested) {
        size_t count = fwrite((const uint8_t *)data + written, 1U,
            requested - written, sink->destination);
        if (count == 0U) {
            return romx_error_set(error, ROMX_E_WRITE, errno,
                sink->bytes_written, "failed to write mutable region");
        }
        written += count;
    }
    sink->bytes_written += size;
    return ROMX_OK;
}

static romx_result_t copy_sink_flush(void *user_data, romx_error_t *error)
{
    mutable_copy_sink_t *sink = (mutable_copy_sink_t *)user_data;
    if (sink == NULL || sink->destination == NULL) {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN, "mutable copy sink is invalid");
    }
    if (fflush(sink->destination) != 0) {
        return romx_error_set(error, ROMX_E_WRITE, errno,
            sink->bytes_written, "failed to flush mutable region");
    }
#if defined(_WIN32)
    if (_commit(_fileno(sink->destination)) != 0) {
#else
    if (fsync(fileno(sink->destination)) != 0) {
#endif
        return romx_error_set(error, ROMX_E_WRITE, errno,
            sink->bytes_written, "failed to sync mutable region");
    }
    return ROMX_OK;
}

static romx_result_t validate_copy_reader(const romx_reader_t *reader,
    romx_info_t *info, romx_error_t *error)
{
    romx_validation_report_t report = ROMX_VALIDATION_REPORT_INIT;
    romx_mutable_status_t status = ROMX_MUTABLE_INVALID;
    uint32_t object_count = UINT32_C(0);
    romx_result_t result;

    result = romx_reader_get_info(reader, info, error);
    if (result != ROMX_OK) return result;
    if (info->mutable_region.size == UINT64_C(0) ||
        info->mutable_region.offset > info->file_size ||
        info->mutable_region.size > info->file_size - info->mutable_region.offset) {
        return romx_error_set(error, ROMX_E_RANGE, 0,
            info->mutable_region.offset, "mutable region is outside the ROMX file");
    }
    result = romx_reader_validate(reader, ROMX_VALIDATE_IMMUTABLE_SHA256,
        &report, error);
    if (result != ROMX_OK) return result;
    result = romx_reader_get_mutable_status(reader, &status, error);
    if (result != ROMX_OK) return result;
    if (status == ROMX_MUTABLE_ABSENT || status == ROMX_MUTABLE_INVALID) {
        return romx_error_set(error, ROMX_E_MUTABLE_HEADER, 0,
            info->mutable_region.offset, "mutable region is not valid");
    }
    /* Enumerating active objects validates each data CRC32 while retaining
     * degraded-but-readable regions exactly as they were supplied. */
    return romx_reader_get_mutable_object_count(reader, &object_count, error);
}

romx_result_t romx_mutable_copy_region_path(
    const char *utf8_source_romx_path,
    const char *utf8_destination_romx_path,
    romx_error_t *error)
{
    romx_reader_options_t reader_options = ROMX_READER_OPTIONS_INIT;
    romx_reader_t *source_reader = NULL;
    romx_reader_t *destination_reader = NULL;
    romx_info_t source_info = ROMX_INFO_INIT;
    romx_info_t destination_info = ROMX_INFO_INIT;
    romx_validation_report_t destination_report = ROMX_VALIDATION_REPORT_INIT;
    romx_mutable_status_t destination_status = ROMX_MUTABLE_INVALID;
    mutable_copy_sink_t sink;
    romx_sink_t output = ROMX_SINK_INIT;
    FILE *destination = NULL;
    romx_result_t result = ROMX_OK;

    romx_error_clear(error);
    if (utf8_source_romx_path == NULL || utf8_destination_romx_path == NULL ||
        utf8_source_romx_path[0] == '\0' || utf8_destination_romx_path[0] == '\0') {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN, "source and destination paths are required");
    }
    if (strcmp(utf8_source_romx_path, utf8_destination_romx_path) == 0) {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN, "mutable source and destination must differ");
    }

    result = romx_reader_open_path(utf8_source_romx_path, &reader_options,
        &source_reader, error);
    if (result != ROMX_OK) goto done;
    result = validate_copy_reader(source_reader, &source_info, error);
    if (result != ROMX_OK) goto done;

    result = romx_reader_open_path(utf8_destination_romx_path, &reader_options,
        &destination_reader, error);
    if (result != ROMX_OK) goto done;
    result = romx_reader_get_info(destination_reader, &destination_info, error);
    if (result != ROMX_OK) goto done;
    if (destination_info.mutable_region.size != source_info.mutable_region.size ||
        destination_info.mutable_region.size == UINT64_C(0) ||
        destination_info.mutable_region.offset > destination_info.file_size ||
        destination_info.mutable_region.size > destination_info.file_size -
            destination_info.mutable_region.offset) {
        result = romx_error_set(error, ROMX_E_MUTABLE_NO_SPACE, 0,
            destination_info.mutable_region.offset,
            "source and destination mutable capacities do not match");
        goto done;
    }
    result = romx_reader_validate(destination_reader,
        ROMX_VALIDATE_IMMUTABLE_SHA256, &destination_report, error);
    if (result != ROMX_OK) goto done;
    result = romx_reader_get_mutable_status(destination_reader,
        &destination_status, error);
    if (result != ROMX_OK) goto done;
    if (destination_status == ROMX_MUTABLE_ABSENT ||
        destination_status == ROMX_MUTABLE_INVALID) {
        result = romx_error_set(error, ROMX_E_MUTABLE_HEADER, 0,
            destination_info.mutable_region.offset,
            "destination mutable region is not valid");
        goto done;
    }

    destination = open_destination(utf8_destination_romx_path);
    if (destination == NULL) {
        result = romx_error_set(error, ROMX_E_IO, errno,
            destination_info.mutable_region.offset,
            "failed to open destination ROMX for mutable copy");
        goto done;
    }
    if (seek_destination(destination, destination_info.mutable_region.offset) != 0) {
        result = romx_error_set(error, ROMX_E_IO, errno,
            destination_info.mutable_region.offset,
            "failed to seek destination mutable region");
        goto done;
    }
    sink = (mutable_copy_sink_t){ destination,
        source_info.mutable_region.size, UINT64_C(0) };
    output = (romx_sink_t)ROMX_SINK_INIT;
    output.user_data = &sink;
    output.write = copy_sink_write;
    output.flush = copy_sink_flush;
    result = romx_reader_copy_region(source_reader, ROMX_REGION_MUTABLE,
        &output, error);
    if (result == ROMX_OK && sink.bytes_written != sink.expected_size) {
        result = romx_error_set(error, ROMX_E_TRUNCATED, 0,
            sink.bytes_written, "mutable region copy was truncated");
    }

done:
    if (destination != NULL && fclose(destination) != 0 && result == ROMX_OK) {
        result = romx_error_set(error, ROMX_E_WRITE, errno,
            destination_info.mutable_region.offset,
            "failed to close destination ROMX after mutable copy");
    }
    romx_reader_close(destination_reader);
    romx_reader_close(source_reader);
    return result;
}
