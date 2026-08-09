# Validation and extraction

Opening a reader validates the ROMX 1.0 footer and region structure. It checks
the magic, version, footer size, flags, optional-region size agreement, range
overflow, bounds, overlap, complete coverage of every byte before the footer,
and the rule that a disabled body SHA-256 field is all zero. When an optional
region size is zero, its offset is ignored. The 32 bytes at footer offset
`0x38` are reserved in ROMX 1.0 and are never interpreted as a payload hash.

Use `romx_reader_validate` for streaming component checks.
`ROMX_VALIDATE_ALL` reports:

- `payload_hashes`: the library calculated the exact payload CRC32 and derived
  SHA-256. ROMX 1.0 stores no payload SHA-256 and does not authenticate one.
- `body_sha256`: the SHA-256 of every byte before the footer when
  `ROMX_FLAG_HAS_BODY_SHA256` is enabled, otherwise `ROMX_STATUS_ABSENT`.
- `metadata`: strict UTF-8 without BOM, RFC 8259 JSON, duplicate-key rejection
  at every object level, and the frozen ROMX 1.0 schema (including the closed
  `cover` object properties).
- `cover`: structural PNG profile validation including signature, limits, IHDR
  first/unique rules, legal color/depth combinations, IDAT/PLTE ordering,
  chunk bounds, critical chunks, final IEND, and chunk CRCs. Pixels are not
  decoded.
- `cover_hashes`: a derived SHA-256 for API compatibility only; no cover hash
  is stored in metadata or required by ROMX 1.0.

Malformed optional metadata or cover data is recorded as
`ROMX_STATUS_INVALID` but does not make the top-level validation call fail.
`metadata_result` and `cover_result` retain the exact component error. An
enabled body SHA-256 mismatch is a container integrity failure and returns
`ROMX_E_BODY_HASH`.

Metadata `crc32` is a required, syntactically valid database lookup identity.
It is not compared with the payload. The report uses
`ROMX_CRC32_VALID_LOOKUP`, `ROMX_CRC32_ABSENT`, or `ROMX_CRC32_INVALID`.
Callers that need the payload's actual CRC32 use
`computed_payload_crc32`; the optional metadata `origin_crc32` is provenance
data and is not substituted for the lookup identity.

`romx_reader_get_payload_format` returns the effective extraction format. For
Game Boy metadata, a payload CGB flag of `0xC0` overrides `gb` with `gbc`;
`0x80` retains the valid metadata choice. The API never uses the container
filename extension for recognition or classification.

`romx_extract_payload_path` first checks the enabled body SHA-256, then streams
the exact payload to a unique temporary file in the destination directory,
checks its derived SHA-256, and atomically publishes it. Invalid optional
metadata or cover regions do not block payload extraction. Existing output is
preserved unless `ROMX_EXTRACT_REPLACE_EXISTING` is set.

`romx_extract_payload_cache` uses:

```text
<computed-payload-sha256>.<effective-payload-format>
```

The full digest is an implementation cache key, not a ROMX field. Metadata
database matching remains the lower-case `crc32` value; `origin_crc32` is
optional provenance. Existing cache entries are checked by size and full
derived SHA-256.
Concurrent writers only publish completely written, verified files. No
playlist, database write, GUI action, or emulator launch is part of this API.

## Frozen conformance corpus

Set `ROMX_CONFORMANCE_FIXTURE_DIR` when the standard repository is not located
next to libromx:

```sh
cmake -S . -B build \
  -DROMX_BUILD_TESTS=ON \
  -DROMX_CONFORMANCE_FIXTURE_DIR=/path/to/romx/tests/fixtures
ctest --test-dir build --output-on-failure
```

The conformance test consumes the checked-in `.romx` bytes without regenerating
them. It covers all frozen reader-open, validation, component-status, reserved
field, lookup CRC32, salvage, and payload-extraction expectations.
