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
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

typedef struct writer_settings {
    romx_writer_flags_t flags;
    const char *lookup_crc32;
    uint64_t max_metadata_size;
    uint64_t max_cover_size;
    uint32_t max_cover_dimension;
    uint32_t io_chunk_size;
} writer_settings_t;

#if defined(_WIN32)
typedef struct writer_file_input {
    HANDLE handle;
    uint64_t size;
} writer_file_input_t;
#else
typedef struct writer_file_input {
    int descriptor;
    uint64_t size;
} writer_file_input_t;
#endif

static void write_le32(uint8_t *output, uint32_t value)
{
    unsigned int index;
    for (index = 0U; index < 4U; ++index) {
        output[index] = (uint8_t)(value >> (index * 8U));
    }
}

static void write_le64(uint8_t *output, uint64_t value)
{
    unsigned int index;
    for (index = 0U; index < 8U; ++index) {
        output[index] = (uint8_t)(value >> (index * 8U));
    }
}

static int writer_settings_load(const romx_writer_options_t *options,
    writer_settings_t *settings)
{
    const romx_writer_flags_t allowed = ROMX_WRITER_BODY_SHA256 |
        ROMX_WRITER_REPLACE_EXISTING | ROMX_WRITER_DURABLE;

    settings->flags = UINT32_C(0);
    settings->lookup_crc32 = NULL;
    settings->max_metadata_size = ROMX_DEFAULT_MAX_METADATA_SIZE;
    settings->max_cover_size = ROMX_DEFAULT_MAX_COVER_SIZE;
    settings->max_cover_dimension = ROMX_DEFAULT_MAX_COVER_DIMENSION;
    settings->io_chunk_size = ROMX_DEFAULT_IO_CHUNK_SIZE;
    if (options == NULL) {
        return 1;
    }
    if (options->struct_size < sizeof(*options) ||
        (options->flags & ~allowed) != UINT32_C(0)) {
        return 0;
    }
    settings->flags = options->flags;
    settings->lookup_crc32 = options->lookup_crc32;
    if (options->max_metadata_size != UINT64_C(0)) {
        settings->max_metadata_size = options->max_metadata_size;
    }
    if (options->max_cover_size != UINT64_C(0)) {
        settings->max_cover_size = options->max_cover_size;
    }
    if (options->max_cover_dimension != UINT32_C(0)) {
        settings->max_cover_dimension = options->max_cover_dimension;
    }
    if (options->io_chunk_size != UINT32_C(0)) {
        settings->io_chunk_size = options->io_chunk_size;
    }
    return settings->io_chunk_size >= UINT32_C(1024) &&
        settings->max_metadata_size != UINT64_C(0) &&
        settings->max_cover_size != UINT64_C(0) &&
        settings->max_cover_dimension != UINT32_C(0);
}

static int valid_input(const romx_io_t *input)
{
    return input != NULL && input->struct_size >= sizeof(*input) &&
        input->get_size != NULL && input->read_at != NULL;
}

static romx_result_t input_size(const romx_io_t *input, uint64_t *size,
    romx_error_t *error)
{
    romx_result_t result = input->get_size(input->user_data, size, error);
    if (result != ROMX_OK) {
        return result;
    }
    return ROMX_OK;
}

static romx_result_t input_read_exact(const romx_io_t *input,
    uint64_t offset, void *buffer, uint64_t size, romx_error_t *error)
{
    uint64_t total = UINT64_C(0);

    while (total < size) {
        uint64_t actual = UINT64_C(0);
        romx_result_t result = input->read_at(input->user_data,
            offset + total, (uint8_t *)buffer + (size_t)total,
            size - total, &actual, error);
        if (result != ROMX_OK) {
            return result;
        }
        if (actual == UINT64_C(0) || actual > size - total) {
            return romx_error_set(error, ROMX_E_TRUNCATED, 0,
                offset + total, "writer input ended before its declared size");
        }
        total += actual;
    }
    return ROMX_OK;
}

#if defined(_WIN32)
static wchar_t *writer_to_wide(const char *path)
{
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        path, -1, NULL, 0);
    wchar_t *wide;
    if (count <= 0) {
        return NULL;
    }
    wide = (wchar_t *)malloc((size_t)count * sizeof(*wide));
    if (wide == NULL) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        path, -1, wide, count) <= 0) {
        free(wide);
        return NULL;
    }
    return wide;
}

static int writer_remove(const char *path)
{
    wchar_t *wide = writer_to_wide(path);
    int result;
    if (wide == NULL) {
        return -1;
    }
    result = _wremove(wide);
    free(wide);
    return result;
}
#else
static int writer_remove(const char *path)
{
    return unlink(path);
}
#endif

static romx_result_t output_write_all(int descriptor, const void *data,
    size_t size, uint64_t output_offset, romx_error_t *error)
{
    size_t written = 0U;

    while (written < size) {
#if defined(_WIN32)
        int actual = _write(descriptor, (const uint8_t *)data + written,
            (unsigned int)(size - written));
#else
        ssize_t actual = write(descriptor,
            (const uint8_t *)data + written, size - written);
#endif
        if (actual < 0) {
            if (errno == EINTR) {
                continue;
            }
            return romx_error_set(error, ROMX_E_WRITE, errno,
                output_offset + (uint64_t)written,
                "failed to write ROMX output");
        }
        if (actual == 0) {
            return romx_error_set(error, ROMX_E_WRITE, 0,
                output_offset + (uint64_t)written,
                "zero-length ROMX output write");
        }
        written += (size_t)actual;
    }
    return ROMX_OK;
}

static romx_result_t stream_input(const romx_io_t *input, uint64_t size,
    int descriptor, uint64_t output_offset, uint32_t chunk_size,
    romx_sha256_context_t *body_sha, romx_sha256_context_t *region_sha,
    uint32_t *region_crc, romx_error_t *error)
{
    uint8_t *buffer = (uint8_t *)malloc(chunk_size);
    uint64_t position = UINT64_C(0);
    romx_result_t result = ROMX_OK;

    if (buffer == NULL) {
        return romx_error_set(error, ROMX_E_OUT_OF_MEMORY, 0,
            ROMX_OFFSET_UNKNOWN, "failed to allocate writer buffer");
    }
    while (position < size) {
        uint64_t count = size - position;
        if (count > chunk_size) {
            count = chunk_size;
        }
        result = input_read_exact(input, position, buffer, count, error);
        if (result != ROMX_OK) {
            break;
        }
        result = output_write_all(descriptor, buffer, (size_t)count,
            output_offset + position, error);
        if (result != ROMX_OK) {
            break;
        }
        romx_sha256_update(body_sha, buffer, (size_t)count);
        if (region_sha != NULL) {
            romx_sha256_update(region_sha, buffer, (size_t)count);
        }
        if (region_crc != NULL) {
            *region_crc = romx_crc32_update(
                *region_crc, buffer, (size_t)count);
        }
        position += count;
    }
    free(buffer);
    return result;
}

static romx_result_t create_temporary(const char *destination,
    char **temporary_out, int *descriptor_out, romx_error_t *error)
{
    static const char suffix[] = ".romx-tmp-XXXXXX";
    size_t length = strlen(destination);
    char *temporary;
    int descriptor;

    *temporary_out = NULL;
    *descriptor_out = -1;
    if (length > SIZE_MAX - sizeof(suffix)) {
        return romx_error_set(error, ROMX_E_OUT_OF_MEMORY, 0,
            ROMX_OFFSET_UNKNOWN, "temporary output path is too long");
    }
    temporary = (char *)malloc(length + sizeof(suffix));
    if (temporary == NULL) {
        return romx_error_set(error, ROMX_E_OUT_OF_MEMORY, 0,
            ROMX_OFFSET_UNKNOWN, "failed to allocate temporary output path");
    }
    memcpy(temporary, destination, length);
    memcpy(temporary + length, suffix, sizeof(suffix));
#if defined(_WIN32)
    descriptor = -1;
    {
        unsigned int attempt;
        char *tail = temporary + length + 10U;
        for (attempt = 0U; attempt < 1000U; ++attempt) {
            wchar_t *wide;
            (void)snprintf(tail, 7U, "%06u", attempt);
            wide = writer_to_wide(temporary);
            if (wide == NULL) {
                errno = EINVAL;
                break;
            }
            {
                int open_error = _wsopen_s(&descriptor, wide,
                    _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY,
                    _SH_DENYNO, _S_IREAD | _S_IWRITE);
                if (open_error != 0) {
                    descriptor = -1;
                    errno = open_error;
                }
            }
            free(wide);
            if (descriptor >= 0 || errno != EEXIST) {
                break;
            }
        }
    }
#else
    descriptor = mkstemp(temporary);
#endif
    if (descriptor < 0) {
        int code = errno;
        free(temporary);
        return romx_error_set(error, ROMX_E_WRITE, code,
            ROMX_OFFSET_UNKNOWN, "failed to create temporary ROMX output");
    }
    *temporary_out = temporary;
    *descriptor_out = descriptor;
    return ROMX_OK;
}

static romx_result_t close_output(int descriptor, int durable,
    romx_error_t *error)
{
    if (durable) {
#if defined(_WIN32)
        if (_commit(descriptor) != 0) {
#else
        if (fsync(descriptor) != 0) {
#endif
            int code = errno;
#if defined(_WIN32)
            (void)_close(descriptor);
#else
            (void)close(descriptor);
#endif
            return romx_error_set(error, ROMX_E_WRITE, code,
                ROMX_OFFSET_UNKNOWN, "failed to sync ROMX output");
        }
    }
#if defined(_WIN32)
    if (_close(descriptor) != 0) {
#else
    if (close(descriptor) != 0) {
#endif
        return romx_error_set(error, ROMX_E_WRITE, errno,
            ROMX_OFFSET_UNKNOWN, "failed to close ROMX output");
    }
    return ROMX_OK;
}

static romx_result_t publish_temporary(const char *temporary,
    const char *destination, int replace, romx_error_t *error)
{
#if defined(_WIN32)
    wchar_t *source = writer_to_wide(temporary);
    wchar_t *target = writer_to_wide(destination);
    DWORD flags = MOVEFILE_WRITE_THROUGH |
        (replace ? MOVEFILE_REPLACE_EXISTING : 0U);
    if (source == NULL || target == NULL ||
        !MoveFileExW(source, target, flags)) {
        DWORD code = GetLastError();
        free(source);
        free(target);
        (void)writer_remove(temporary);
        return romx_error_set(error,
            code == ERROR_ALREADY_EXISTS || code == ERROR_FILE_EXISTS
                ? ROMX_E_EXISTS : ROMX_E_ATOMIC_RENAME,
            (int32_t)code, ROMX_OFFSET_UNKNOWN,
            "failed to publish ROMX output");
    }
    free(source);
    free(target);
#else
    if (replace) {
        if (rename(temporary, destination) != 0) {
            int code = errno;
            (void)unlink(temporary);
            return romx_error_set(error, ROMX_E_ATOMIC_RENAME, code,
                ROMX_OFFSET_UNKNOWN, "failed to replace ROMX output");
        }
    } else {
        if (link(temporary, destination) != 0) {
            int code = errno;
            (void)unlink(temporary);
            return romx_error_set(error,
                code == EEXIST ? ROMX_E_EXISTS : ROMX_E_ATOMIC_RENAME,
                code, ROMX_OFFSET_UNKNOWN, "failed to publish ROMX output");
        }
        (void)unlink(temporary);
    }
#endif
    return ROMX_OK;
}

romx_result_t romx_writer_write_io_path(const char *destination,
    const romx_io_t *payload, const void *metadata_json,
    uint64_t metadata_input_size, const romx_io_t *cover,
    const romx_writer_options_t *options, romx_writer_report_t *report,
    romx_error_t *error)
{
    writer_settings_t settings;
    uint64_t payload_size = UINT64_C(0);
    uint64_t cover_size = UINT64_C(0);
    uint8_t *metadata = NULL;
    size_t metadata_size = 0U;
    char *temporary = NULL;
    int descriptor = -1;
    romx_sha256_context_t body_context;
    romx_sha256_context_t payload_context;
    uint32_t payload_crc = romx_crc32_begin();
    uint8_t payload_sha256[32];
    uint8_t body_sha256[32];
    uint8_t validated_cover_sha256[32];
    uint8_t written_cover_sha256[32];
    uint32_t cover_width = UINT32_C(0);
    uint32_t cover_height = UINT32_C(0);
    uint8_t footer[ROMX_FOOTER_SIZE_0_1_0];
    uint32_t flags = UINT32_C(0);
    uint64_t body_size;
    uint64_t file_size;
    uint64_t metadata_offset;
    uint64_t cover_offset;
    uint32_t supplied_report_size = 0U;
    romx_result_t result;

    romx_error_clear(error);
    if (destination == NULL || destination[0] == '\0' ||
        !valid_input(payload) ||
        (metadata_json == NULL) != (metadata_input_size == UINT64_C(0)) ||
        (cover != NULL && !valid_input(cover)) ||
        !writer_settings_load(options, &settings)) {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN, "invalid ROMX writer arguments");
    }
    if (report != NULL) {
        supplied_report_size = report->struct_size;
        if (supplied_report_size < sizeof(*report)) {
            return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
                ROMX_OFFSET_UNKNOWN, "writer report structure is too small");
        }
        memset(report, 0, sizeof(*report));
        report->struct_size = supplied_report_size;
    }
    if (metadata_json == NULL && settings.lookup_crc32 != NULL) {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN,
            "lookup CRC32 override requires metadata");
    }
    if (metadata_input_size > settings.max_metadata_size ||
        metadata_input_size > (uint64_t)SIZE_MAX) {
        return romx_error_set(error, ROMX_E_METADATA_TOO_LARGE, 0,
            UINT64_C(0), "writer metadata exceeds configured limit");
    }

    result = input_size(payload, &payload_size, error);
    if (result != ROMX_OK) {
        return result;
    }
    if (payload_size == UINT64_C(0)) {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            UINT64_C(0), "ROM payload must not be empty");
    }
    if (cover != NULL) {
        result = input_size(cover, &cover_size, error);
        if (result != ROMX_OK) {
            return result;
        }
        if (cover_size == UINT64_C(0)) {
            return romx_error_set(error, ROMX_E_COVER_PNG, 0,
                UINT64_C(0), "cover must not be empty");
        }
        result = romx_validate_cover_io(cover, cover_size,
            settings.max_cover_size, settings.max_cover_dimension,
            settings.io_chunk_size, validated_cover_sha256,
            &cover_width, &cover_height, error);
        if (result != ROMX_OK) {
            return result;
        }
    }
    if (payload_size > UINT64_MAX - ROMX_FOOTER_SIZE_0_1_0 ||
        cover_size > UINT64_MAX - payload_size - ROMX_FOOTER_SIZE_0_1_0) {
        return romx_error_set(error, ROMX_E_RANGE, 0,
            ROMX_OFFSET_UNKNOWN, "ROMX writer input sizes overflow");
    }

    result = create_temporary(destination, &temporary, &descriptor, error);
    if (result != ROMX_OK) {
        return result;
    }
    romx_sha256_init(&body_context);
    romx_sha256_init(&payload_context);
    result = stream_input(payload, payload_size, descriptor, UINT64_C(0),
        settings.io_chunk_size, &body_context, &payload_context,
        &payload_crc, error);
    if (result != ROMX_OK) {
        goto fail;
    }
    romx_sha256_finish(&payload_context, payload_sha256);
    payload_crc = romx_crc32_finish(payload_crc);

    if (metadata_json != NULL) {
        result = romx_prepare_metadata((const uint8_t *)metadata_json,
            (size_t)metadata_input_size, payload_crc,
            settings.lookup_crc32, cover != NULL, cover_width,
            cover_height, &metadata, &metadata_size, error);
        if (result != ROMX_OK) {
            goto fail;
        }
        if ((uint64_t)metadata_size > settings.max_metadata_size) {
            result = romx_error_set(error, ROMX_E_METADATA_TOO_LARGE, 0,
                payload_size, "generated metadata exceeds configured limit");
            goto fail;
        }
        result = output_write_all(descriptor, metadata, metadata_size,
            payload_size, error);
        if (result != ROMX_OK) {
            goto fail;
        }
        romx_sha256_update(&body_context, metadata, metadata_size);
        flags |= ROMX_FLAG_HAS_METADATA;
    }
    metadata_offset = metadata_size != 0U ? payload_size : UINT64_C(0);
    if ((uint64_t)metadata_size > UINT64_MAX - payload_size) {
        result = romx_error_set(error, ROMX_E_RANGE, 0,
            ROMX_OFFSET_UNKNOWN, "ROMX metadata size overflows");
        goto fail;
    }
    cover_offset = cover_size != UINT64_C(0)
        ? payload_size + (uint64_t)metadata_size : UINT64_C(0);
    if (cover != NULL) {
        romx_sha256_context_t cover_context;
        romx_sha256_init(&cover_context);
        if (cover_offset > UINT64_MAX - cover_size ||
            cover_offset + cover_size > UINT64_MAX - ROMX_FOOTER_SIZE_0_1_0) {
            result = romx_error_set(error, ROMX_E_RANGE, 0,
                ROMX_OFFSET_UNKNOWN, "ROMX cover size overflows");
            goto fail;
        }
        result = stream_input(cover, cover_size, descriptor, cover_offset,
            settings.io_chunk_size, &body_context, &cover_context, NULL, error);
        if (result != ROMX_OK) {
            goto fail;
        }
        romx_sha256_finish(&cover_context, written_cover_sha256);
        if (memcmp(validated_cover_sha256,
            written_cover_sha256, 32U) != 0) {
            result = romx_error_set(error, ROMX_E_COVER_PNG, 0,
                cover_offset, "cover input changed after PNG validation");
            goto fail;
        }
        flags |= ROMX_FLAG_HAS_COVER;
    }
    body_size = payload_size + (uint64_t)metadata_size + cover_size;
    if (body_size > UINT64_MAX - ROMX_FOOTER_SIZE_0_1_0) {
        result = romx_error_set(error, ROMX_E_RANGE, 0,
            ROMX_OFFSET_UNKNOWN, "ROMX output size overflows");
        goto fail;
    }
    file_size = body_size + ROMX_FOOTER_SIZE_0_1_0;
    romx_sha256_finish(&body_context, body_sha256);

    memset(footer, 0, sizeof(footer));
    memcpy(footer, "ROMX", 4U);
    write_le32(footer + 0x04U, ROMX_FORMAT_VERSION_0_1_0);
    write_le64(footer + 0x08U, UINT64_C(0));
    write_le64(footer + 0x10U, payload_size);
    write_le64(footer + 0x18U, metadata_offset);
    write_le64(footer + 0x20U, (uint64_t)metadata_size);
    write_le64(footer + 0x28U, cover_offset);
    write_le64(footer + 0x30U, cover_size);
    if ((settings.flags & ROMX_WRITER_BODY_SHA256) != 0U) {
        flags |= ROMX_FLAG_HAS_BODY_SHA256;
        memcpy(footer + 0x60U, body_sha256, 32U);
    }
    write_le32(footer + 0x58U, flags);
    write_le32(footer + 0x5cU, ROMX_FOOTER_SIZE_0_1_0);
    result = output_write_all(descriptor, footer, sizeof(footer),
        body_size, error);
    if (result != ROMX_OK) {
        goto fail;
    }
    result = close_output(descriptor,
        (settings.flags & ROMX_WRITER_DURABLE) != 0U, error);
    descriptor = -1;
    if (result != ROMX_OK) {
        goto fail;
    }
    result = publish_temporary(temporary, destination,
        (settings.flags & ROMX_WRITER_REPLACE_EXISTING) != 0U, error);
    if (result != ROMX_OK) {
        free(metadata);
        free(temporary);
        return result;
    }

    if (report != NULL) {
        report->flags = flags;
        report->file_size = file_size;
        report->body_size = body_size;
        report->payload_size = payload_size;
        report->metadata_size = (uint64_t)metadata_size;
        report->cover_size = cover_size;
        report->payload_crc32 = payload_crc;
        memcpy(report->payload_sha256, payload_sha256, 32U);
        if ((flags & ROMX_FLAG_HAS_BODY_SHA256) != 0U) {
            memcpy(report->body_sha256, body_sha256, 32U);
        }
    }
    free(metadata);
    free(temporary);
    romx_error_clear(error);
    return ROMX_OK;

fail:
    if (descriptor >= 0) {
#if defined(_WIN32)
        (void)_close(descriptor);
#else
        (void)close(descriptor);
#endif
    }
    if (temporary != NULL) {
        (void)writer_remove(temporary);
    }
    free(metadata);
    free(temporary);
    return result;
}

#if defined(_WIN32)
static romx_result_t writer_file_get_size(void *user, uint64_t *size,
    romx_error_t *error)
{
    writer_file_input_t *input = (writer_file_input_t *)user;
    (void)error;
    *size = input->size;
    return ROMX_OK;
}

static romx_result_t writer_file_read_at(void *user, uint64_t offset,
    void *buffer, uint64_t size, uint64_t *bytes_read, romx_error_t *error)
{
    writer_file_input_t *input = (writer_file_input_t *)user;
    uint8_t *output = (uint8_t *)buffer;
    *bytes_read = UINT64_C(0);
    while (*bytes_read < size) {
        OVERLAPPED operation;
        DWORD count;
        DWORD actual = 0U;
        uint64_t position = offset + *bytes_read;
        uint64_t remaining = size - *bytes_read;
        count = remaining > UINT32_MAX ? UINT32_MAX : (DWORD)remaining;
        memset(&operation, 0, sizeof(operation));
        operation.Offset = (DWORD)position;
        operation.OffsetHigh = (DWORD)(position >> 32);
        if (!ReadFile(input->handle, output + (size_t)*bytes_read,
            count, &actual, &operation)) {
            DWORD code = GetLastError();
            if (code != ERROR_IO_PENDING ||
                !GetOverlappedResult(input->handle, &operation,
                    &actual, TRUE)) {
                code = GetLastError();
                return romx_error_set(error, ROMX_E_IO, (int32_t)code,
                    position, "failed to read writer input");
            }
        }
        *bytes_read += actual;
        if (actual != count) {
            break;
        }
    }
    return ROMX_OK;
}
#else
static romx_result_t writer_file_get_size(void *user, uint64_t *size,
    romx_error_t *error)
{
    writer_file_input_t *input = (writer_file_input_t *)user;
    (void)error;
    *size = input->size;
    return ROMX_OK;
}

static romx_result_t writer_file_read_at(void *user, uint64_t offset,
    void *buffer, uint64_t size, uint64_t *bytes_read, romx_error_t *error)
{
    writer_file_input_t *input = (writer_file_input_t *)user;
    uint8_t *output = (uint8_t *)buffer;
    *bytes_read = UINT64_C(0);
    if (offset > (uint64_t)INT64_MAX) {
        return romx_error_set(error, ROMX_E_RANGE, 0, offset,
            "writer input offset exceeds platform limit");
    }
    while (*bytes_read < size) {
        uint64_t remaining = size - *bytes_read;
        size_t count = remaining > (uint64_t)SSIZE_MAX
            ? (size_t)SSIZE_MAX : (size_t)remaining;
        ssize_t actual = pread(input->descriptor,
            output + (size_t)*bytes_read, count,
            (off_t)(offset + *bytes_read));
        if (actual < 0) {
            if (errno == EINTR) {
                continue;
            }
            return romx_error_set(error, ROMX_E_IO, errno,
                offset + *bytes_read, "failed to read writer input");
        }
        if (actual == 0) {
            break;
        }
        *bytes_read += (uint64_t)actual;
    }
    return ROMX_OK;
}
#endif

static romx_result_t open_writer_file(const char *path,
    writer_file_input_t **state_out, romx_io_t *io, romx_error_t *error)
{
    writer_file_input_t *state;

    if (path == NULL || path[0] == '\0') {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN, "writer input path is empty");
    }
    state = (writer_file_input_t *)calloc(1U, sizeof(*state));
    if (state == NULL) {
        return romx_error_set(error, ROMX_E_OUT_OF_MEMORY, 0,
            ROMX_OFFSET_UNKNOWN, "failed to allocate writer file state");
    }
#if defined(_WIN32)
    {
        LARGE_INTEGER size;
        wchar_t *wide = writer_to_wide(path);
        if (wide == NULL) {
            free(state);
            return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
                ROMX_OFFSET_UNKNOWN, "writer path is not valid UTF-8");
        }
        state->handle = CreateFileW(wide, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL |
            FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED, NULL);
        free(wide);
        if (state->handle == INVALID_HANDLE_VALUE ||
            !GetFileSizeEx(state->handle, &size)) {
            DWORD code = GetLastError();
            if (state->handle != INVALID_HANDLE_VALUE) {
                CloseHandle(state->handle);
            }
            free(state);
            return romx_error_set(error, ROMX_E_IO, (int32_t)code,
                ROMX_OFFSET_UNKNOWN, "failed to open writer input path");
        }
        state->size = (uint64_t)size.QuadPart;
    }
#else
    {
        struct stat status;
        state->descriptor = open(path, O_RDONLY);
        if (state->descriptor < 0 || fstat(state->descriptor, &status) != 0 ||
            status.st_size < 0) {
            int code = errno;
            if (state->descriptor >= 0) {
                (void)close(state->descriptor);
            }
            free(state);
            return romx_error_set(error, ROMX_E_IO, code,
                ROMX_OFFSET_UNKNOWN, "failed to open writer input path");
        }
        state->size = (uint64_t)status.st_size;
    }
#endif
    *io = (romx_io_t)ROMX_IO_INIT;
    io->user_data = state;
    io->get_size = writer_file_get_size;
    io->read_at = writer_file_read_at;
    *state_out = state;
    return ROMX_OK;
}

static void close_writer_file(writer_file_input_t *state)
{
    if (state == NULL) {
        return;
    }
#if defined(_WIN32)
    if (state->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(state->handle);
    }
#else
    if (state->descriptor >= 0) {
        (void)close(state->descriptor);
    }
#endif
    free(state);
}

romx_result_t romx_writer_write_paths(const char *destination,
    const char *payload_path, const char *metadata_path,
    const char *cover_path, const romx_writer_options_t *options,
    romx_writer_report_t *report, romx_error_t *error)
{
    writer_settings_t settings;
    writer_file_input_t *payload_state = NULL;
    writer_file_input_t *metadata_state = NULL;
    writer_file_input_t *cover_state = NULL;
    romx_io_t payload_io = ROMX_IO_INIT;
    romx_io_t metadata_io = ROMX_IO_INIT;
    romx_io_t cover_io = ROMX_IO_INIT;
    uint8_t *metadata = NULL;
    uint64_t metadata_size = UINT64_C(0);
    romx_result_t result;

    if (destination == NULL || payload_path == NULL ||
        !writer_settings_load(options, &settings)) {
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN, "invalid writer path arguments");
    }
    result = open_writer_file(payload_path, &payload_state,
        &payload_io, error);
    if (result != ROMX_OK) {
        goto done;
    }
    if (metadata_path != NULL) {
        result = open_writer_file(metadata_path, &metadata_state,
            &metadata_io, error);
        if (result != ROMX_OK) {
            goto done;
        }
        result = input_size(&metadata_io, &metadata_size, error);
        if (result != ROMX_OK) {
            goto done;
        }
        if (metadata_size == UINT64_C(0) ||
            metadata_size > settings.max_metadata_size ||
            metadata_size > (uint64_t)SIZE_MAX) {
            result = romx_error_set(error, ROMX_E_METADATA_TOO_LARGE, 0,
                UINT64_C(0), "metadata path is empty or exceeds configured limit");
            goto done;
        }
        metadata = (uint8_t *)malloc((size_t)metadata_size);
        if (metadata == NULL) {
            result = romx_error_set(error, ROMX_E_OUT_OF_MEMORY, 0,
                ROMX_OFFSET_UNKNOWN, "failed to allocate metadata input");
            goto done;
        }
        result = input_read_exact(&metadata_io, UINT64_C(0), metadata,
            metadata_size, error);
        if (result != ROMX_OK) {
            goto done;
        }
    }
    if (cover_path != NULL) {
        result = open_writer_file(cover_path, &cover_state, &cover_io, error);
        if (result != ROMX_OK) {
            goto done;
        }
    }
    result = romx_writer_write_io_path(destination, &payload_io,
        metadata, metadata_size, cover_state != NULL ? &cover_io : NULL,
        options, report, error);

done:
    free(metadata);
    close_writer_file(cover_state);
    close_writer_file(metadata_state);
    close_writer_file(payload_state);
    return result;
}
