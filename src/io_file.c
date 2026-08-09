#if !defined(_WIN32)
#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#endif

#include "romx_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>

typedef struct romx_file_io { HANDLE handle; uint64_t size; } romx_file_io_t;

static wchar_t *romx_utf8_to_wide(const char *path)
{
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    wchar_t *wide;
    if (count <= 0) return NULL;
    wide = (wchar_t *)malloc((size_t)count * sizeof(*wide));
    if (wide == NULL) return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, count) <= 0) {
        free(wide); return NULL;
    }
    return wide;
}

static void romx_file_close(void *user_data)
{
    romx_file_io_t *state = (romx_file_io_t *)user_data;
    if (state != NULL) { if (state->handle != INVALID_HANDLE_VALUE) CloseHandle(state->handle); free(state); }
}

static romx_result_t romx_file_get_size(void *user_data, uint64_t *size, romx_error_t *error)
{
    romx_file_io_t *state = (romx_file_io_t *)user_data;
    if (state == NULL || size == NULL) return romx_error_set(error,
        ROMX_E_INVALID_ARGUMENT, 0, ROMX_OFFSET_UNKNOWN, "invalid file I/O state");
    *size = state->size;
    return ROMX_OK;
}

static romx_result_t romx_file_read_at(void *user_data, uint64_t offset,
    void *buffer, uint64_t size, uint64_t *bytes_read, romx_error_t *error)
{
    romx_file_io_t *state = (romx_file_io_t *)user_data;
    uint8_t *output = (uint8_t *)buffer;
    *bytes_read = 0U;
    while (*bytes_read < size) {
        OVERLAPPED overlapped;
        DWORD actual = 0U;
        DWORD count = (DWORD)((size - *bytes_read) > UINT32_MAX ? UINT32_MAX : size - *bytes_read);
        uint64_t position = offset + *bytes_read;
        memset(&overlapped, 0, sizeof(overlapped));
        overlapped.Offset = (DWORD)position;
        overlapped.OffsetHigh = (DWORD)(position >> 32);
        if (!ReadFile(state->handle, output + (size_t)*bytes_read, count, &actual, &overlapped)) {
            DWORD code = GetLastError();
            if (code == ERROR_IO_PENDING) {
                if (!GetOverlappedResult(state->handle, &overlapped, &actual, TRUE))
                    code = GetLastError();
                else code = ERROR_SUCCESS;
            }
            if (code == ERROR_HANDLE_EOF) return ROMX_OK;
            if (code != ERROR_SUCCESS) return romx_error_set(error, ROMX_E_IO,
                (int32_t)code, position, "failed to read ROMX file");
        }
        *bytes_read += actual;
        if (actual != count) break;
    }
    return ROMX_OK;
}

#else

#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct romx_file_io { int descriptor; uint64_t size; } romx_file_io_t;

static void romx_file_close(void *user_data)
{
    romx_file_io_t *state = (romx_file_io_t *)user_data;
    if (state != NULL) { if (state->descriptor >= 0) (void)close(state->descriptor); free(state); }
}

static romx_result_t romx_file_get_size(void *user_data, uint64_t *size, romx_error_t *error)
{
    romx_file_io_t *state = (romx_file_io_t *)user_data;
    if (state == NULL || size == NULL) return romx_error_set(error,
        ROMX_E_INVALID_ARGUMENT, 0, ROMX_OFFSET_UNKNOWN, "invalid file I/O state");
    *size = state->size;
    return ROMX_OK;
}

static romx_result_t romx_file_read_at(void *user_data, uint64_t offset,
    void *buffer, uint64_t size, uint64_t *bytes_read, romx_error_t *error)
{
    romx_file_io_t *state = (romx_file_io_t *)user_data;
    uint8_t *output = (uint8_t *)buffer;
    *bytes_read = UINT64_C(0);
    if (offset > (uint64_t)INT64_MAX) return romx_error_set(error,
        ROMX_E_RANGE, 0, offset, "file offset exceeds platform limit");
    while (*bytes_read < size) {
        uint64_t remaining = size - *bytes_read;
        size_t count = remaining > (uint64_t)SSIZE_MAX ? (size_t)SSIZE_MAX : (size_t)remaining;
        ssize_t actual = pread(state->descriptor, output + (size_t)*bytes_read,
            count, (off_t)(offset + *bytes_read));
        if (actual < 0) {
            if (errno == EINTR) continue;
            return romx_error_set(error, ROMX_E_IO, errno, offset + *bytes_read,
                "failed to read ROMX file");
        }
        if (actual == 0) break;
        *bytes_read += (uint64_t)actual;
    }
    return ROMX_OK;
}
#endif

romx_result_t romx_reader_open_path(const char *utf8_path,
    const romx_reader_options_t *options, romx_reader_t **out_reader,
    romx_error_t *error)
{
    romx_file_io_t *state;
    romx_io_t io = ROMX_IO_INIT;
    romx_result_t result;
    romx_error_clear(error);
    if (out_reader != NULL) *out_reader = NULL;
    if (utf8_path == NULL || utf8_path[0] == '\0' || out_reader == NULL)
        return romx_error_set(error, ROMX_E_INVALID_ARGUMENT, 0,
            ROMX_OFFSET_UNKNOWN, "path and out_reader must not be null or empty");
    state = (romx_file_io_t *)calloc(1U, sizeof(*state));
    if (state == NULL) return romx_error_set(error, ROMX_E_OUT_OF_MEMORY, 0,
        ROMX_OFFSET_UNKNOWN, "failed to allocate file I/O state");
#if defined(_WIN32)
    {
        LARGE_INTEGER size;
        wchar_t *wide = romx_utf8_to_wide(utf8_path);
        if (wide == NULL) { free(state); return romx_error_set(error,
            ROMX_E_INVALID_ARGUMENT, (int32_t)GetLastError(), ROMX_OFFSET_UNKNOWN,
            "path is not valid UTF-8"); }
        state->handle = CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS |
            FILE_FLAG_OVERLAPPED, NULL);
        free(wide);
        if (state->handle == INVALID_HANDLE_VALUE || !GetFileSizeEx(state->handle, &size)) {
            DWORD code = GetLastError(); romx_file_close(state);
            return romx_error_set(error, ROMX_E_IO, (int32_t)code,
                ROMX_OFFSET_UNKNOWN, "failed to open or size ROMX file");
        }
        state->size = (uint64_t)size.QuadPart;
    }
#else
    {
        struct stat status;
        state->descriptor = open(utf8_path, O_RDONLY);
        if (state->descriptor < 0 || fstat(state->descriptor, &status) != 0 || status.st_size < 0) {
            int code = errno; romx_file_close(state);
            return romx_error_set(error, ROMX_E_IO, code,
                ROMX_OFFSET_UNKNOWN, "failed to open or size ROMX file");
        }
        state->size = (uint64_t)status.st_size;
    }
#endif
    io.user_data = state; io.get_size = romx_file_get_size; io.read_at = romx_file_read_at;
    result = romx_reader_create(&io, options, romx_file_close, out_reader, error);
    if (result != ROMX_OK) romx_file_close(state);
    return result;
}
