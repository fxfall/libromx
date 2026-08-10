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
