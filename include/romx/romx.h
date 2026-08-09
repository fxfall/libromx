#ifndef ROMX_ROMX_H
#define ROMX_ROMX_H

#include <stddef.h>
#include <stdint.h>

#include <romx/romx_version.h>

#if defined(_WIN32) && defined(ROMX_SHARED)
#  if defined(ROMX_BUILDING_LIBRARY)
#    define ROMX_API __declspec(dllexport)
#  else
#    define ROMX_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) || defined(__clang__)
#  define ROMX_API __attribute__((visibility("default")))
#else
#  define ROMX_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ROMX_FORMAT_VERSION_1 UINT32_C(1)
#define ROMX_FOOTER_SIZE_V1 UINT32_C(128)

#define ROMX_FLAG_HAS_METADATA UINT32_C(0x00000001)
#define ROMX_FLAG_HAS_COVER UINT32_C(0x00000002)
#define ROMX_FLAG_HAS_BODY_SHA256 UINT32_C(0x00000004)
#define ROMX_FLAGS_V1_MASK UINT32_C(0x00000007)

#define ROMX_ERROR_MESSAGE_CAPACITY 256
#define ROMX_OFFSET_UNKNOWN UINT64_MAX

typedef int32_t romx_result_t;

#define ROMX_OK                         INT32_C(0)
#define ROMX_E_INVALID_ARGUMENT        INT32_C(-1)
#define ROMX_E_OUT_OF_MEMORY           INT32_C(-2)
#define ROMX_E_IO                      INT32_C(-3)
#define ROMX_E_TRUNCATED               INT32_C(-4)
#define ROMX_E_INVALID_FOOTER          INT32_C(-5)
#define ROMX_E_INVALID_FLAGS           INT32_C(-6)
#define ROMX_E_RANGE                   INT32_C(-7)
#define ROMX_E_OVERLAP                 INT32_C(-8)
#define ROMX_E_BODY_HASH               INT32_C(-9)
#define ROMX_E_METADATA_ABSENT         INT32_C(-10)
#define ROMX_E_METADATA_TOO_LARGE      INT32_C(-11)
#define ROMX_E_METADATA_UTF8           INT32_C(-12)
#define ROMX_E_METADATA_JSON           INT32_C(-13)
#define ROMX_E_METADATA_SCHEMA         INT32_C(-14)
#define ROMX_E_COVER_ABSENT            INT32_C(-15)
#define ROMX_E_COVER_TOO_LARGE         INT32_C(-16)
#define ROMX_E_COVER_PNG               INT32_C(-17)
#define ROMX_E_EXTRACT_HASH            INT32_C(-18)
#define ROMX_E_BUFFER_TOO_SMALL        INT32_C(-19)
#define ROMX_E_WRITE                   INT32_C(-20)
#define ROMX_E_ATOMIC_RENAME           INT32_C(-21)
#define ROMX_E_EXISTS                  INT32_C(-22)

typedef struct romx_error {
    romx_result_t code;
    int32_t system_code;
    uint64_t byte_offset;
    char message[ROMX_ERROR_MESSAGE_CAPACITY];
} romx_error_t;

typedef struct romx_io {
    uint32_t struct_size;
    void *user_data;
    romx_result_t (*get_size)(
        void *user_data,
        uint64_t *size,
        romx_error_t *error);
    romx_result_t (*read_at)(
        void *user_data,
        uint64_t offset,
        void *buffer,
        uint64_t size,
        uint64_t *bytes_read,
        romx_error_t *error);
} romx_io_t;

#define ROMX_IO_INIT { (uint32_t)sizeof(romx_io_t), NULL, NULL, NULL }

typedef struct romx_reader_options {
    uint32_t struct_size;
    uint32_t reserved;
    uint64_t max_metadata_size;
    uint64_t max_cover_size;
    uint32_t max_cover_dimension;
    uint32_t io_chunk_size;
} romx_reader_options_t;

#define ROMX_READER_OPTIONS_INIT \
    { (uint32_t)sizeof(romx_reader_options_t), UINT32_C(0), \
      UINT64_C(0), UINT64_C(0), UINT32_C(0), UINT32_C(0) }

#define ROMX_DEFAULT_MAX_METADATA_SIZE UINT64_C(1048576)
#define ROMX_DEFAULT_MAX_COVER_SIZE UINT64_C(33554432)
#define ROMX_DEFAULT_MAX_COVER_DIMENSION UINT32_C(8192)
#define ROMX_DEFAULT_IO_CHUNK_SIZE UINT32_C(65536)

typedef struct romx_region_info {
    uint64_t offset;
    uint64_t size;
} romx_region_info_t;

typedef struct romx_info {
    uint32_t struct_size;
    uint32_t version;
    uint64_t file_size;
    uint64_t body_size;
    romx_region_info_t rom;
    romx_region_info_t metadata;
    romx_region_info_t cover;
    uint8_t reserved[32];
    uint32_t flags;
    uint32_t footer_size;
    uint8_t body_sha256[32];
} romx_info_t;

#define ROMX_INFO_INIT { \
    (uint32_t)sizeof(romx_info_t), UINT32_C(0), UINT64_C(0), UINT64_C(0), \
    { UINT64_C(0), UINT64_C(0) }, \
    { UINT64_C(0), UINT64_C(0) }, \
    { UINT64_C(0), UINT64_C(0) }, \
    { 0 }, UINT32_C(0), UINT32_C(0), { 0 } \
}

typedef struct romx_reader romx_reader_t;
typedef struct romx_metadata romx_metadata_t;

typedef int32_t romx_region_t;
#define ROMX_REGION_ROM       INT32_C(1)
#define ROMX_REGION_METADATA  INT32_C(2)
#define ROMX_REGION_COVER     INT32_C(3)
#define ROMX_REGION_BODY      INT32_C(4)

typedef uint32_t romx_validate_flags_t;
#define ROMX_VALIDATE_PAYLOAD_HASHES UINT32_C(0x00000001) /* derived values; no payload hash is stored */
#define ROMX_VALIDATE_BODY_SHA256 UINT32_C(0x00000002)
#define ROMX_VALIDATE_METADATA    UINT32_C(0x00000004)
#define ROMX_VALIDATE_COVER       UINT32_C(0x00000008)
#define ROMX_VALIDATE_ALL         UINT32_C(0x0000000f)

typedef int32_t romx_validation_status_t;
#define ROMX_STATUS_NOT_CHECKED INT32_C(0)
#define ROMX_STATUS_VALID       INT32_C(1)
#define ROMX_STATUS_INVALID     INT32_C(2)
#define ROMX_STATUS_ABSENT      INT32_C(3)

typedef int32_t romx_crc32_status_t;
#define ROMX_CRC32_NOT_CHECKED INT32_C(0)
#define ROMX_CRC32_ABSENT      INT32_C(1)
#define ROMX_CRC32_VALID_LOOKUP INT32_C(2)
#define ROMX_CRC32_INVALID      INT32_C(3)

typedef struct romx_validation_report {
    uint32_t struct_size;
    romx_validation_status_t structure;
    romx_validation_status_t payload_hashes; /* derived CRC/digest calculation */
    romx_validation_status_t body_sha256;
    romx_validation_status_t metadata;
    romx_validation_status_t cover;
    romx_validation_status_t cover_hashes; /* derived API value; no cover hash is normative */
    romx_result_t metadata_result;
    romx_result_t cover_result;
    romx_crc32_status_t metadata_crc32;
    uint32_t computed_payload_crc32;
    uint8_t computed_payload_sha256[32]; /* derived only; never a ROMX 1.0 field */
    uint8_t computed_body_sha256[32];
    uint8_t computed_cover_sha256[32]; /* derived only; never metadata */
    uint32_t cover_width;
    uint32_t cover_height;
} romx_validation_report_t;

#define ROMX_VALIDATION_REPORT_INIT { \
    (uint32_t)sizeof(romx_validation_report_t), \
    ROMX_STATUS_NOT_CHECKED, ROMX_STATUS_NOT_CHECKED, \
    ROMX_STATUS_NOT_CHECKED, ROMX_STATUS_NOT_CHECKED, \
    ROMX_STATUS_NOT_CHECKED, ROMX_STATUS_NOT_CHECKED, \
    ROMX_OK, ROMX_OK, \
    ROMX_CRC32_NOT_CHECKED, UINT32_C(0), { 0 }, { 0 }, { 0 }, \
    UINT32_C(0), UINT32_C(0) \
}

typedef struct romx_sink {
    uint32_t struct_size;
    void *user_data;
    romx_result_t (*write)(
        void *user_data,
        const void *data,
        uint64_t size,
        romx_error_t *error);
    romx_result_t (*flush)(void *user_data, romx_error_t *error);
} romx_sink_t;

#define ROMX_SINK_INIT { (uint32_t)sizeof(romx_sink_t), NULL, NULL, NULL }

typedef uint32_t romx_extract_flags_t;
#define ROMX_EXTRACT_REPLACE_EXISTING UINT32_C(0x00000001)
#define ROMX_EXTRACT_DURABLE          UINT32_C(0x00000002)

typedef struct romx_extract_options {
    uint32_t struct_size;
    romx_extract_flags_t flags;
} romx_extract_options_t;

#define ROMX_EXTRACT_OPTIONS_INIT \
    { (uint32_t)sizeof(romx_extract_options_t), UINT32_C(0) }

typedef struct romx_cover_info {
    uint32_t struct_size;
    uint32_t width;
    uint32_t height;
    uint64_t size;
    uint8_t sha256[32]; /* derived API value; not stored in ROMX metadata */
} romx_cover_info_t;

#define ROMX_COVER_INFO_INIT \
    { (uint32_t)sizeof(romx_cover_info_t), UINT32_C(0), UINT32_C(0), \
      UINT64_C(0), { 0 } }

typedef uint32_t romx_writer_flags_t;
#define ROMX_WRITER_BODY_SHA256      UINT32_C(0x00000001)
#define ROMX_WRITER_REPLACE_EXISTING UINT32_C(0x00000002)
#define ROMX_WRITER_DURABLE          UINT32_C(0x00000004)

typedef struct romx_writer_options {
    uint32_t struct_size;
    romx_writer_flags_t flags;
    const char *lookup_crc32;
    uint64_t max_metadata_size;
    uint64_t max_cover_size;
    uint32_t max_cover_dimension;
    uint32_t io_chunk_size;
} romx_writer_options_t;

#define ROMX_WRITER_OPTIONS_INIT { \
    (uint32_t)sizeof(romx_writer_options_t), UINT32_C(0), NULL, \
    UINT64_C(0), UINT64_C(0), UINT32_C(0), UINT32_C(0) \
}

typedef struct romx_writer_report {
    uint32_t struct_size;
    uint32_t flags;
    uint64_t file_size;
    uint64_t body_size;
    uint64_t payload_size;
    uint64_t metadata_size;
    uint64_t cover_size;
    uint32_t payload_crc32;
    uint8_t payload_sha256[32]; /* derived report value; not written to footer */
    uint8_t body_sha256[32];
} romx_writer_report_t;

#define ROMX_WRITER_REPORT_INIT { \
    (uint32_t)sizeof(romx_writer_report_t), UINT32_C(0), \
    UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0), UINT64_C(0), \
    UINT32_C(0), { 0 }, { 0 } \
}

ROMX_API const char *romx_result_string(romx_result_t result);

ROMX_API romx_result_t romx_reader_open_path(
    const char *utf8_path,
    const romx_reader_options_t *options,
    romx_reader_t **out_reader,
    romx_error_t *error);

ROMX_API romx_result_t romx_reader_open_io(
    const romx_io_t *io,
    const romx_reader_options_t *options,
    romx_reader_t **out_reader,
    romx_error_t *error);

ROMX_API romx_result_t romx_reader_get_info(
    const romx_reader_t *reader,
    romx_info_t *info,
    romx_error_t *error);

ROMX_API romx_result_t romx_reader_read_region(
    const romx_reader_t *reader,
    romx_region_t region,
    uint64_t region_offset,
    void *buffer,
    uint64_t buffer_size,
    uint64_t *bytes_read,
    romx_error_t *error);

ROMX_API romx_result_t romx_reader_copy_region(
    const romx_reader_t *reader,
    romx_region_t region,
    const romx_sink_t *sink,
    romx_error_t *error);

ROMX_API romx_result_t romx_reader_validate(
    const romx_reader_t *reader,
    romx_validate_flags_t flags,
    romx_validation_report_t *report,
    romx_error_t *error);

ROMX_API romx_result_t romx_metadata_open(
    const romx_reader_t *reader,
    romx_metadata_t **out_metadata,
    romx_error_t *error);

ROMX_API void romx_metadata_close(romx_metadata_t *metadata);

ROMX_API romx_result_t romx_metadata_copy_json(
    const romx_metadata_t *metadata,
    void *buffer,
    uint64_t capacity,
    uint64_t *required_size,
    romx_error_t *error);

ROMX_API romx_result_t romx_metadata_get_string(
    const romx_metadata_t *metadata,
    const char *key,
    char *buffer,
    uint64_t capacity,
    uint64_t *required_size,
    romx_error_t *error);

ROMX_API romx_result_t romx_metadata_get_value_json(
    const romx_metadata_t *metadata,
    const char *key,
    void *buffer,
    uint64_t capacity,
    uint64_t *required_size,
    romx_error_t *error);

ROMX_API romx_result_t romx_reader_get_payload_format(
    const romx_reader_t *reader,
    char *buffer,
    uint64_t capacity,
    uint64_t *required_size,
    romx_error_t *error);

ROMX_API romx_result_t romx_extract_payload_path(
    const romx_reader_t *reader,
    const char *utf8_destination_path,
    const romx_extract_options_t *options,
    romx_error_t *error);

ROMX_API romx_result_t romx_extract_payload_cache(
    const romx_reader_t *reader,
    const char *utf8_cache_directory,
    const romx_extract_options_t *options,
    char *result_path,
    uint64_t result_capacity,
    uint64_t *required_size,
    romx_error_t *error);

ROMX_API romx_result_t romx_reader_get_cover_info(
    const romx_reader_t *reader,
    romx_cover_info_t *info,
    romx_error_t *error);

ROMX_API romx_result_t romx_extract_cover_path(
    const romx_reader_t *reader,
    const char *utf8_destination_path,
    const romx_extract_options_t *options,
    romx_error_t *error);

ROMX_API romx_result_t romx_writer_write_io_path(
    const char *utf8_destination_path,
    const romx_io_t *payload,
    const void *metadata_json,
    uint64_t metadata_size,
    const romx_io_t *cover,
    const romx_writer_options_t *options,
    romx_writer_report_t *report,
    romx_error_t *error);

ROMX_API romx_result_t romx_writer_write_paths(
    const char *utf8_destination_path,
    const char *utf8_payload_path,
    const char *utf8_metadata_path,
    const char *utf8_cover_path,
    const romx_writer_options_t *options,
    romx_writer_report_t *report,
    romx_error_t *error);

ROMX_API void romx_reader_close(romx_reader_t *reader);

#ifdef __cplusplus
}
#endif

#endif
