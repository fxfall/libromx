# Integration guide

## C and C++

Install libromx and consume its exported CMake target:

```cmake
find_package(romx CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE ROMX::romx)
```

Include `<romx/romx.h>` from C99 or later. C++ applications may use the same C
ABI directly or include `<romx/romx.hpp>` for the optional C++11 RAII wrapper.
The wrapper is header-only and does not change the core library ABI.

Initialize every public options/report structure with its matching `*_INIT`
macro. Check every returned `romx_result_t`; `romx_error_t` supplies the system
code, byte offset, and a bounded diagnostic message.

## Rust and other FFI consumers

Bind only `romx/romx.h`. Public structures use fixed-width integers, pointers,
and callbacks; no C++ type crosses the ABI. Opaque reader and metadata handles
must be closed with their matching function. A Rust wrapper should mark a
custom `romx_io_t` as concurrently usable only when its `read_at` callback and
user state are thread-safe.

## Frontends and emulator adapters

libromx reads, validates, writes, extracts, and exposes a bounded virtual view
of the payload. A frontend may use either of these integration modes:

### Direct payload view

Use `romx_reader_get_payload_io` when a core or its file loader can consume
random-access callbacks:

```c
romx_reader_t *container = NULL;
romx_io_t iso = ROMX_IO_INIT;
uint64_t iso_size = 0;
romx_error_t error = {0};

if (romx_reader_open_path("game.isox", NULL, &container, &error) == ROMX_OK &&
    romx_reader_get_payload_io(container, &iso, &error) == ROMX_OK &&
    iso.get_size(iso.user_data, &iso_size, &error) == ROMX_OK) {
    /* Give iso.get_size and iso.read_at to the core's FileLoader/VFS. */
}

/* The borrowed iso view must no longer be used after this call. */
romx_reader_close(container);
```

The returned `romx_io_t` acts as a read-only virtual file:

- virtual byte zero maps to `rom_offset` from the validated footer;
- virtual size is exactly `rom_size`;
- reads crossing the payload end are shortened like regular file reads;
- reads at or beyond the payload end return zero bytes;
- metadata, cover, and footer bytes are never visible;
- random reads allocate no buffer inside libromx and never read the complete
  payload unless the caller explicitly requests every byte.

This is the preferred low-memory integration for large `.isox` payloads. A
PPSSPP-style file loader should implement its size and positional-read methods
with this view. It may keep its own small fixed-size cache, but must not allocate
`rom_size` bytes merely to open the image.

`romx_io_t` is a callback interface, not an operating-system pathname. A core
that accepts only a real path must either be adapted at its file-loader/VFS
layer or use the extraction mode below. libromx does not mount a virtual file
or claim that the `.isox` container itself is a raw ISO path.

### Guarded payload mapping

`romx_reader_map_payload` creates an independently owned, read-only payload
mapping for APIs such as `retro_game_info.data`. Full aligned payload pages are
file-backed. At most two partial boundary pages are copied into anonymous
memory so bytes belonging to metadata, cover, or the footer are not exposed.
Guard pages surround the mapping. Closing the reader does not invalidate a
successful mapping; release it with `romx_payload_mapping_close`.

Mapping is an optional capability of path-backed readers. A custom
`romx_io_t` source, an address-space limit, or a platform without the guarded
mapping backend returns `ROMX_E_UNSUPPORTED` or `ROMX_E_RANGE`; callers should
then use bounded payload I/O. If the container enables body SHA-256, mapping
validates it first. With the default disabled flag, mapping does not scan the
complete payload at startup.

### Extracted payload path

Use extraction for an unmodified path-only core:

1. Open and validate the ROMX file.
2. Extract or reuse the atomically verified payload cache entry.
3. Pass only the extracted raw payload path to the emulator core.
4. Read the embedded cover and metadata independently for presentation.

Do not pass the complete ROMX container as though it were a raw image. Pass
either the bounded payload view or an extracted raw payload path. Do not
generate LPL data in the core library or store UI/database state in ROMX
metadata.

## Streaming and lifetime

Path APIs own their file handles for the duration of the call or reader handle.
For callback APIs, the caller owns `user_data` and must keep it valid until the
operation finishes. Callback `get_size` must remain stable during a writer call,
and `read_at` must return bytes from the same immutable input snapshot.

Readers contain immutable parsed state and the built-in path backend supports
concurrent positional reads. Writer calls have no shared mutable global state;
separate calls can run concurrently. Concurrent calls targeting the same output
or cache path are resolved by atomic publication.

A payload view borrows its reader. Its callbacks may be called concurrently
under the same rules as `romx_reader_read_region`: the built-in path reader is
safe, while a caller-supplied source must provide a thread-safe `read_at`.

## Cover conversion

The writer accepts only validated PNG bytes. Applications that accept JPEG,
WebP, GIF, BMP, resizing, or color conversion should perform that work before
calling libromx and keep the image dependency in a separate optional module.
