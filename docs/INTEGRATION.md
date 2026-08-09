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

libromx only reads, validates, writes, and extracts the container. A frontend
should:

1. Open and validate the ROMX file.
2. Extract or reuse the atomically verified payload cache entry.
3. Pass only the extracted raw payload path to the emulator core.
4. Read the embedded cover and metadata independently for presentation.

Do not pass the ROMX container itself to an emulator, generate LPL data in the
core library, or store UI/database state in ROMX metadata.

## Streaming and lifetime

Path APIs own their file handles for the duration of the call or reader handle.
For callback APIs, the caller owns `user_data` and must keep it valid until the
operation finishes. Callback `get_size` must remain stable during a writer call,
and `read_at` must return bytes from the same immutable input snapshot.

Readers contain immutable parsed state and the built-in path backend supports
concurrent positional reads. Writer calls have no shared mutable global state;
separate calls can run concurrently. Concurrent calls targeting the same output
or cache path are resolved by atomic publication.

## Cover conversion

The writer accepts only validated PNG bytes. Applications that accept JPEG,
WebP, GIF, BMP, resizing, or color conversion should perform that work before
calling libromx and keep the image dependency in a separate optional module.
