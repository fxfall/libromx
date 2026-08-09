# Phases 6 through 8

These phases implement the ROMX 1.0 writer and its integration surface. The
frozen standard repository is the only compatibility target.

## Phase 6: streaming container writer

- Accept payloads through path or positional callback I/O.
- Preserve every payload byte and support 64-bit sizes.
- Write the canonical order: payload, metadata, cover, footer.
- Encode the fixed little-endian 128-byte footer and zero all reserved bytes.
- Keep body SHA-256 disabled by default; calculate and store it when requested.
- Write to a unique temporary file in the target directory and atomically
  publish it, with explicit replace and durable-write options.
- Return payload CRC32/SHA-256 and final region sizes without storing a payload
  SHA-256 in the ROMX footer.

## Phase 7: metadata and cover production

- Parse metadata templates as strict RFC 8259 UTF-8 JSON without BOM; reject
  duplicate keys at every object level.
- Reject every top-level field outside the frozen ROMX 1.0 schema.
- Generate metadata `crc32` from the exact payload by default.
- Accept an explicit lookup CRC32 override and canonicalize it to lowercase.
- When `origin_crc32` is present, regenerate it from the exact payload and
  never replace it with the lookup override.
- Produce compact metadata JSON and regenerate the descriptive cover object
  from the validated PNG dimensions.
- Structurally validate PNG covers before writing (first/unique IHDR, legal
  color/depth, required consecutive IDAT, final IEND, chunk bounds and CRCs)
  and embed their bytes without conversion or modification.
- Validate the closed metadata `cover` object (`mime_type`, `width`, and
  `height` only; no additional properties).
- Enforce configurable metadata, cover, dimension, and I/O chunk limits.

## Phase 8: reusable integration surface

- Expose writer functionality through stable C99 structures and functions.
- Keep the C core independent of C++, RetroArch, databases, frontends, GUI,
  image conversion, and emulator launch behavior.
- Provide an optional header-only C++11 RAII wrapper.
- Provide buildable C and C++ writer examples, error handling documentation,
  and integration guidance.
- Test path and callback writers, CRC semantics, body hashing, atomic failure
  behavior, round-trip extraction, large logical input, concurrency, package
  consumption, the complete frozen reader corpus, and byte-exact writer golden
  fixtures.
