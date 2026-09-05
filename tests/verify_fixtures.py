#!/usr/bin/env python3
"""Regenerate the independent fixtures and compare them with the frozen copy."""

from __future__ import annotations

import filecmp
import subprocess
import sys
import tempfile
from pathlib import Path


def fixture_files(root: Path) -> set[Path]:
    # The specification repository also keeps explanatory README files in this
    # directory; the reference writer intentionally generates only vectors.
    return {
        path.relative_to(root)
        for path in root.rglob("*")
        if path.is_file() and path.name not in {"README.md", "README_CN.md"}
    }


def files_match(expected: Path, generated: Path, relative: Path) -> bool:
    # Path.write_text() uses the host's native line endings.  Manifest files
    # are text vectors, so their line endings must not make the check fail on
    # Windows; ROMX payloads and every other fixture remain byte-for-byte.
    if relative.name.endswith(".manifest.json"):
        return expected.read_bytes().replace(b"\r\n", b"\n") == generated.read_bytes().replace(
            b"\r\n", b"\n"
        )
    return filecmp.cmp(expected, generated, shallow=False)


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} SPEC_ROOT", file=sys.stderr)
        return 2
    spec_root = Path(sys.argv[1]).resolve()
    expected = spec_root / "tests" / "fixtures"
    tool = spec_root / "tools" / "romx_reference.py"
    with tempfile.TemporaryDirectory(prefix="romx-reference-") as temporary:
        generated = Path(temporary)
        subprocess.run(
            [sys.executable, str(tool), "fixtures", str(generated)],
            check=True,
        )
        expected_files = fixture_files(expected)
        generated_files = fixture_files(generated)
        if expected_files != generated_files:
            print("frozen fixture file set differs", file=sys.stderr)
            print("missing:", sorted(expected_files - generated_files), file=sys.stderr)
            print("extra:", sorted(generated_files - expected_files), file=sys.stderr)
            return 1
        mismatches = [
            relative
            for relative in sorted(expected_files)
            if not files_match(expected / relative, generated / relative, relative)
        ]
        if mismatches:
            print("frozen fixture contents differ:", file=sys.stderr)
            for relative in mismatches:
                print(f"  {relative}", file=sys.stderr)
            return 1
    print("frozen ROMX fixtures match the independent reference writer")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
