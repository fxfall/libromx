# ROMX writer API

The writer has two entry points:

- `romx_writer_write_paths` reads payload, optional metadata, and optional PNG
  cover from UTF-8 paths.
- `romx_writer_write_io_path` reads the payload and optional cover through
  positional `romx_io_t` callbacks. Metadata remains an in-memory JSON region
  because ROMX implementations cap it at a small configurable size.

Both functions stream large payloads, write canonical region order, and publish
the completed file atomically. They do not modify the payload and do not write
a payload SHA-256 into the footer.

## Options

Initialize options with `ROMX_WRITER_OPTIONS_INIT`.

- `ROMX_WRITER_BODY_SHA256` enables SHA-256 of every byte before the footer.
  It is disabled by default and the footer field is then all zero.
- `ROMX_WRITER_REPLACE_EXISTING` atomically replaces an existing destination.
  Without it, an existing destination returns `ROMX_E_EXISTS` unchanged.
- `ROMX_WRITER_DURABLE` flushes the temporary output before publication.
- `lookup_crc32` optionally supplies an eight-digit database lookup identity.
  Uppercase input is accepted and written in lowercase.
- Zero limit fields select the documented reader defaults.

## Metadata template behavior

A template must contain `schema_version`, `name`, `platform`, and
`payload_format`. Its `crc32` may be absent or stale. The writer calculates the
payload CRC32 while streaming and inserts or replaces `crc32`. Without an
override it writes the calculated payload value. With an override it writes the
override as the lookup value.

If the template contains `origin_crc32`, the writer always replaces it with
the calculated payload value. This keeps lookup identity and payload provenance
separate. Insignificant JSON whitespace is removed for canonical output. When a
cover is supplied, the writer regenerates the descriptive `cover` object from
the validated PNG dimensions. The generated metadata is parsed and
schema-validated again before it is written.

## Cover behavior

The cover input must already be PNG. libromx validates its signature, chunk
bounds and CRCs, requires a first-and-unique legal IHDR, requires consecutive
IDAT chunks and a final IEND with no trailing bytes, and checks legal PNG
color/depth combinations and PLTE rules. Valid PNG bytes are embedded exactly
as supplied. When metadata is present, its descriptive cover object is generated
as `image/png` with the validated width and height. Image conversion belongs in
an optional adapter outside the core library.

## Errors and atomicity

Every failure returns a `romx_result_t` and fills `romx_error_t`; the library
never exits the process. Invalid metadata and PNG input fail before publication.
Write, sync, footer, or publication failures remove the temporary file. An
existing destination is not changed unless replacement was explicitly enabled.

`romx_writer_report_t` reports final sizes, footer flags, actual payload CRC32,
derived payload SHA-256, and the body SHA-256 when enabled.
