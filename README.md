# libromx

`libromx` is an independent, reusable C99 library for the stable, frozen ROMX
1.0 container format. Its public ABI does not expose C++ STL types and does not depend on an
emulator, frontend, GUI, database, or RetroArch playlist.

**Release status: libromx 0.1.0 complete.** This release targets only the
stable, frozen ROMX 1.0 specification and its frozen reader/writer corpora.

The project currently implements phases 1 through 8:

- ROMX 1.0 footer discovery at end of file;
- UTF-8 path and callback-based input;
- explicit little-endian decoding;
- 64-bit offsets and sizes;
- magic, version, footer-size, flag, range, overlap, and complete body-coverage validation;
- format recognition independent of the filename extension;
- RetroArch-compatible payload CRC32 calculation (lower-case eight-digit metadata formatting);
- optional container body SHA-256 validation;
- strict RFC 8259 UTF-8/JSON, recursive duplicate-key rejection, and ROMX metadata schema validation;
- exact reader-side metadata JSON preservation and field access through opaque handles;
- streaming region reads and callback sinks;
- verified, atomic payload extraction and content-addressed caching;
- structural PNG profile validation (IHDR/IDAT/IEND, legal color/depth, chunk CRCs), and extraction;
- direct execution of the standard repository's frozen conformance fixtures;
- streaming path and callback writers with canonical layout and atomic publish;
- automatic metadata lookup CRC32 generation and optional override;
- compact canonical writer metadata and generated PNG cover descriptors;
- exact PNG cover embedding after structural validation;
- an optional header-only C++11 RAII wrapper and buildable examples;
- byte-exact validation against the frozen reader and writer fixture corpora.

Image conversion remains intentionally outside the core library.

ROMX 1.0 compatibility changes are limited to new conformance fixtures or
clarifying text that does not change byte semantics. Footer layout, field
meaning, or binary-validity changes require a new ROMX format version.
Metadata schema versions evolve independently; an unsupported metadata schema
may be skipped while the payload remains extractable.

## Build and test

```sh
cmake -S . -B build -DROMX_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The frozen standard corpus is enabled automatically when the standard
repository is adjacent to this project. Otherwise configure
`ROMX_CONFORMANCE_FIXTURE_DIR=/path/to/romx/tests/fixtures`.

The callback API is useful for custom storage and frontend VFS adapters. A
successful `romx_reader_open_io` call means that the ROMX footer and complete
region structure are valid. An enabled body hash is checked by validation and
before extraction; the payload CRC32 is calculated on request. The
callback functions and their `user_data` must remain valid
until `romx_reader_close`; libromx does not take ownership of callback state.

For the same reader to be used concurrently, a custom `romx_io_t` implementation
must make `read_at` thread-safe. The built-in path reader uses positional I/O
and supports concurrent reads. Reader metadata is immutable after open.

See [docs/VALIDATION_AND_EXTRACTION.md](docs/VALIDATION_AND_EXTRACTION.md) for
validation, optional-region, CRC32, and cache semantics.
See [docs/WRITER.md](docs/WRITER.md) and
[docs/PHASES_6_TO_8.md](docs/PHASES_6_TO_8.md) for writer semantics and the
completed phase objectives.
See [docs/INTEGRATION.md](docs/INTEGRATION.md) for C, C++, Rust, frontend, and
callback lifetime guidance.
See [docs/RELEASE_0.1.0.md](docs/RELEASE_0.1.0.md) for the frozen release scope
and verification record.
