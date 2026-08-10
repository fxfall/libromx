#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include <romx/romx.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

typedef struct memory_io {
    const uint8_t *data;
    uint64_t size;
    uint64_t last_offset;
    uint64_t last_size;
} memory_io_t;

typedef struct sparse_io {
    uint8_t footer[ROMX_FOOTER_SIZE_0_1_0];
    uint64_t payload_size;
    uint64_t last_offset;
} sparse_io_t;

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

static romx_result_t memory_get_size(
    void *user_data, uint64_t *size, romx_error_t *error)
{
    memory_io_t *memory = (memory_io_t *)user_data;
    (void)error;
    *size = memory->size;
    return ROMX_OK;
}

static romx_result_t memory_read_at(
    void *user_data, uint64_t offset, void *buffer, uint64_t size,
    uint64_t *bytes_read, romx_error_t *error)
{
    memory_io_t *memory = (memory_io_t *)user_data;
    uint64_t available;
    (void)error;
    memory->last_offset = offset;
    memory->last_size = size;
    *bytes_read = UINT64_C(0);
    if (offset >= memory->size || size == UINT64_C(0)) {
        return ROMX_OK;
    }
    available = memory->size - offset;
    if (size > available) {
        size = available;
    }
    memcpy(buffer, memory->data + (size_t)offset, (size_t)size);
    *bytes_read = size;
    return ROMX_OK;
}

static romx_result_t sparse_get_size(
    void *user_data, uint64_t *size, romx_error_t *error)
{
    sparse_io_t *sparse = (sparse_io_t *)user_data;
    (void)error;
    *size = sparse->payload_size + ROMX_FOOTER_SIZE_0_1_0;
    return ROMX_OK;
}

static romx_result_t sparse_read_at(
    void *user_data, uint64_t offset, void *buffer, uint64_t size,
    uint64_t *bytes_read, romx_error_t *error)
{
    sparse_io_t *sparse = (sparse_io_t *)user_data;
    uint64_t index;
    (void)error;
    sparse->last_offset = offset;
    *bytes_read = UINT64_C(0);
    if (offset == sparse->payload_size &&
        size == ROMX_FOOTER_SIZE_0_1_0) {
        memcpy(buffer, sparse->footer, ROMX_FOOTER_SIZE_0_1_0);
        *bytes_read = ROMX_FOOTER_SIZE_0_1_0;
        return ROMX_OK;
    }
    if (offset >= sparse->payload_size) {
        return ROMX_OK;
    }
    if (size > sparse->payload_size - offset) {
        size = sparse->payload_size - offset;
    }
    for (index = UINT64_C(0); index < size; ++index) {
        ((uint8_t *)buffer)[(size_t)index] =
            (uint8_t)((offset + index) & UINT64_C(0xff));
    }
    *bytes_read = size;
    return ROMX_OK;
}

static void make_container(uint8_t *file, size_t size)
{
    static const uint8_t iso[] = {
        0x49, 0x53, 0x4f, 0x2d, 0x50, 0x41, 0x59, 0x4c, 0x4f, 0x41, 0x44
    };
    static const uint8_t metadata[] = { '{', '}', '\n' };
    uint8_t *footer;

    memset(file, 0, size);
    memcpy(file, metadata, sizeof(metadata));
    memcpy(file + sizeof(metadata), iso, sizeof(iso));
    footer = file + sizeof(metadata) + sizeof(iso);
    memcpy(footer, "ROMX", 4U);
    le32(footer + 0x04U, ROMX_FORMAT_VERSION_0_1_0);
    le64(footer + 0x08U, sizeof(metadata));
    le64(footer + 0x10U, sizeof(iso));
    le64(footer + 0x18U, 0U);
    le64(footer + 0x20U, sizeof(metadata));
    le32(footer + 0x58U, ROMX_FLAG_HAS_METADATA);
    le32(footer + 0x5cU, ROMX_FOOTER_SIZE_0_1_0);
}

static void test_payload_view(void)
{
    enum { ISO_SIZE = 11, METADATA_SIZE = 3 };
    uint8_t file[METADATA_SIZE + ISO_SIZE + ROMX_FOOTER_SIZE_0_1_0];
    uint8_t output[32];
    memory_io_t memory;
    romx_io_t source = ROMX_IO_INIT;
    romx_io_t payload = ROMX_IO_INIT;
    romx_reader_t *reader = NULL;
    romx_payload_mapping_t *mapping = NULL;
    romx_error_t error;
    uint64_t size = UINT64_C(0);
    uint64_t bytes_read = UINT64_C(0);

    make_container(file, sizeof(file));
    memory.data = file;
    memory.size = sizeof(file);
    memory.last_offset = UINT64_C(0);
    memory.last_size = UINT64_C(0);
    source.user_data = &memory;
    source.get_size = memory_get_size;
    source.read_at = memory_read_at;

    CHECK(romx_reader_open_io(&source, NULL, &reader, &error) == ROMX_OK);
    CHECK(romx_reader_get_payload_io(reader, &payload, &error) == ROMX_OK);
    CHECK(payload.get_size(payload.user_data, &size, &error) == ROMX_OK);
    CHECK(size == ISO_SIZE);

    memset(output, 0, sizeof(output));
    CHECK(payload.read_at(payload.user_data, 0U, output, sizeof(output),
        &bytes_read, &error) == ROMX_OK);
    CHECK(bytes_read == ISO_SIZE);
    CHECK(memcmp(output, "ISO-PAYLOAD", ISO_SIZE) == 0);
    CHECK(memory.last_offset == METADATA_SIZE);
    CHECK(memory.last_size == ISO_SIZE);

    memset(output, 0, sizeof(output));
    CHECK(payload.read_at(payload.user_data, 4U, output, 3U,
        &bytes_read, &error) == ROMX_OK);
    CHECK(bytes_read == 3U && memcmp(output, "PAY", 3U) == 0);
    CHECK(memory.last_offset == METADATA_SIZE + 4U);

    CHECK(payload.read_at(payload.user_data, ISO_SIZE - 2U, output, 8U,
        &bytes_read, &error) == ROMX_OK);
    CHECK(bytes_read == 2U && memcmp(output, "AD", 2U) == 0);
    CHECK(payload.read_at(payload.user_data, ISO_SIZE, output, 1U,
        &bytes_read, &error) == ROMX_OK);
    CHECK(bytes_read == 0U);
    CHECK(payload.read_at(payload.user_data, ISO_SIZE + 100U, output, 1U,
        &bytes_read, &error) == ROMX_OK);
    CHECK(bytes_read == 0U);

    CHECK(romx_reader_map_payload(reader, &mapping, &error) ==
        ROMX_E_UNSUPPORTED);
    CHECK(mapping == NULL);
    CHECK(strcmp(romx_result_string(ROMX_E_UNSUPPORTED),
        "operation is not supported for this input") == 0);

    payload.struct_size = 0U;
    CHECK(romx_reader_get_payload_io(reader, &payload, &error) ==
        ROMX_E_INVALID_ARGUMENT);
    romx_reader_close(reader);
}

static void test_large_sparse_payload(void)
{
    const uint64_t payload_size = UINT64_C(5) * UINT64_C(1024) *
        UINT64_C(1024) * UINT64_C(1024) + UINT64_C(17);
    const uint64_t offset = UINT64_C(4) * UINT64_C(1024) *
        UINT64_C(1024) * UINT64_C(1024) + UINT64_C(9);
    sparse_io_t sparse;
    romx_io_t source = ROMX_IO_INIT;
    romx_io_t payload = ROMX_IO_INIT;
    romx_reader_t *reader = NULL;
    romx_error_t error;
    uint8_t output[4] = { 0 };
    uint64_t size = UINT64_C(0);
    uint64_t bytes_read = UINT64_C(0);

    memset(&sparse, 0, sizeof(sparse));
    sparse.payload_size = payload_size;
    memcpy(sparse.footer, "ROMX", 4U);
    le32(sparse.footer + 0x04U, ROMX_FORMAT_VERSION_0_1_0);
    le64(sparse.footer + 0x08U, 0U);
    le64(sparse.footer + 0x10U, payload_size);
    le32(sparse.footer + 0x5cU, ROMX_FOOTER_SIZE_0_1_0);
    source.user_data = &sparse;
    source.get_size = sparse_get_size;
    source.read_at = sparse_read_at;

    CHECK(romx_reader_open_io(&source, NULL, &reader, &error) == ROMX_OK);
    CHECK(romx_reader_get_payload_io(reader, &payload, &error) == ROMX_OK);
    CHECK(payload.get_size(payload.user_data, &size, &error) == ROMX_OK);
    CHECK(size == payload_size);
    CHECK(payload.read_at(payload.user_data, offset, output, sizeof(output),
        &bytes_read, &error) == ROMX_OK);
    CHECK(bytes_read == sizeof(output));
    CHECK(sparse.last_offset == offset);
    CHECK(output[0] == 9U && output[1] == 10U &&
        output[2] == 11U && output[3] == 12U);
    romx_reader_close(reader);
}

#if !defined(_WIN32)
static void test_guarded_payload_mapping(void)
{
    const size_t metadata_size = 3U;
    const size_t payload_size = 9001U;
    const size_t file_size = metadata_size + payload_size + ROMX_FOOTER_SIZE_0_1_0;
    uint8_t *file = (uint8_t *)calloc(1U, file_size);
    uint8_t *footer;
    char path[] = "/tmp/libromx-map-XXXXXX";
    int descriptor;
    size_t index;
    size_t written = 0U;
    romx_reader_t *reader = NULL;
    romx_payload_mapping_t *mapping = NULL;
    romx_error_t error;
    const uint8_t *data;

    CHECK(file != NULL);
    if (file == NULL) return;
    memcpy(file, "{}\n", metadata_size);
    for (index = 0U; index < payload_size; ++index)
        file[metadata_size + index] = (uint8_t)((index * 37U + 11U) & 0xffU);
    footer = file + metadata_size + payload_size;
    memcpy(footer, "ROMX", 4U);
    le32(footer + 0x04U, ROMX_FORMAT_VERSION_0_1_0);
    le64(footer + 0x08U, metadata_size);
    le64(footer + 0x10U, payload_size);
    le64(footer + 0x18U, 0U);
    le64(footer + 0x20U, metadata_size);
    le32(footer + 0x58U, ROMX_FLAG_HAS_METADATA);
    le32(footer + 0x5cU, ROMX_FOOTER_SIZE_0_1_0);

    descriptor = mkstemp(path);
    CHECK(descriptor >= 0);
    if (descriptor < 0) { free(file); return; }
    while (written < file_size) {
        ssize_t actual = write(descriptor, file + written, file_size - written);
        CHECK(actual > 0);
        if (actual <= 0) break;
        written += (size_t)actual;
    }
    CHECK(close(descriptor) == 0);
    CHECK(written == file_size);

    CHECK(romx_reader_open_path(path, NULL, &reader, &error) == ROMX_OK);
    CHECK(romx_reader_map_payload(reader, &mapping, &error) == ROMX_OK);
    CHECK(romx_payload_mapping_size(mapping) == payload_size);
    data = (const uint8_t *)romx_payload_mapping_data(mapping);
    CHECK(data != NULL);
    CHECK(memcmp(data, file + metadata_size, payload_size) == 0);

    romx_reader_close(reader);
    reader = NULL;
    CHECK(data[0] == file[metadata_size]);
    CHECK(data[payload_size - 1U] == file[metadata_size + payload_size - 1U]);

    romx_payload_mapping_close(mapping);
    CHECK(unlink(path) == 0);
    free(file);
}
#endif

int main(void)
{
    test_payload_view();
    test_large_sparse_payload();
#if !defined(_WIN32)
    test_guarded_payload_mapping();
#endif
    if (failures != 0) {
        return EXIT_FAILURE;
    }
    puts("all payload view tests passed");
    return EXIT_SUCCESS;
}
