# libromx 0.1.0

Status: complete against the stable, frozen ROMX 0.1.0 specification.

This release supports ROMX 0.1.0 (footer wire code 1) and metadata schema 0.1.0. It
does not contain a compatibility parser for an earlier container format.

## Completed scope

- Streaming C99 reader and writer APIs with 64-bit offsets and callback I/O.
- Strict footer, region, body coverage, flags, and optional body SHA-256 checks.
- RFC 8259 metadata parsing, recursive duplicate-key rejection, closed schema
  validation, automatic CRC32 fields, and canonical compact writer output.
- Structural ROMX PNG profile validation and byte-exact cover handling.
- Byte-exact payload extraction and SHA-256-addressed atomic cache publication.
- Header-only C++11 wrapper, CMake package, examples, and integration docs.

## Release verification

- All 26 frozen reader fixtures pass.
- All 7 frozen writer golden fixtures match byte-for-byte.
- All phase tests pass for static and shared builds.
- Strict warning-as-error C99/C++11 builds pass.
- AddressSanitizer and UndefinedBehaviorSanitizer tests pass.
- ThreadSanitizer reader and concurrent-writer tests pass.
- The shared library exports only the 20 documented public C functions.
- An installed-package consumer links and runs successfully.
- A real ROMX produced by libromx passes the standard Python verifier.

The completed runtime verification environment was macOS arm64. The source has
POSIX and Windows backends; native Linux and Windows CI remain release-platform
validation rather than ROMX format work.
