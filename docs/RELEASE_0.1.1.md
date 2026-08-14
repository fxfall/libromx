# libromx 0.1.1

libromx 0.1.1 is an API and integration release for the frozen ROMX 0.1.0
container format. It does not change the footer, metadata schema, region
semantics, writer output, or conformance corpus.

The release adds a footer-bounded random-access payload view, an independently
owned guarded read-only payload mapping on supported path-backed platforms,
and explicit `ROMX_E_UNSUPPORTED` fallback reporting.

Mappings validate an enabled body SHA-256 before exposing bytes. When body
SHA-256 is absent, opening and mapping do not force a complete payload scan.
Partial filesystem pages are copied into isolated anonymous pages so adjacent
container regions are not exposed through the returned payload pointer.

The release also provides an independent read-only `romx_payload_file_t`
cursor. It exposes payload-relative `read`, `seek`, `tell`, and `size`
operations without exposing metadata, cover, or footer bytes. Each cursor owns
its reader and position, allowing frontend VFS handles to remain independent
and safe for concurrent access. Body SHA-256 validation is opt-in.

`romx_metadata_get_crc32()` provides the canonical metadata lookup CRC32 as a
native `uint32_t`, while the generic metadata JSON/string APIs remain
available for frontend-specific fields.
