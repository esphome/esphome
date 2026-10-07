#!/usr/bin/env python3
"""Rewrite namespace-scope log TAG declarations in component sources to ESPHOME_LOG_TAG.

On ESP8266 ESPHOME_LOG_TAG keeps the tag in flash. Files that use TAG for anything other
than logging (scheduler names, string functions) are skipped and listed.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent.parent

DECL = re.compile(
    r"^(?:static const char \*const|static constexpr const char \*const|"
    r"constexpr static const char \*const|static constexpr const char \*|"
    r"static const char \*|const char \*const|constexpr const char \*const)"
    r" ?TAG = (\"[^\"\n]*\");$",
    re.MULTILINE,
)
# A flash tag must never reach code that reads it as a normal string
UNSAFE = re.compile(
    r"(?:set_timeout|set_interval|set_retry|cancel_timeout|cancel_interval|cancel_retry|defer)"
    r"\s*\(\s*TAG\b|\b(?:strcmp|strncmp|strlen|strcpy|strncpy|strcat)\s*\([^;\n]*\bTAG\b|"
    r"std::string\s*[({]\s*TAG\b|\bstd::string\b[^;\n]*=\s*TAG\s*;"
)


def convert(path: Path) -> str:
    """Return 'converted', 'none' or a skip reason."""
    text = path.read_text(encoding="utf-8")
    if "#undef TAG" in text:
        return "skipped: #undef TAG"
    matches = DECL.findall(text)
    if not matches:
        return "none"
    if len(matches) > 1:
        return "skipped: more than one TAG declaration"
    if UNSAFE.search(text):
        return "skipped: TAG used as a string outside logging"
    new = DECL.sub(lambda m: f"ESPHOME_LOG_TAG(TAG, {m.group(1)});", text, count=1)
    path.write_text(new, encoding="utf-8")
    return "converted"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "paths", nargs="*", type=Path, help="files to convert (default: all components)"
    )
    args = parser.parse_args()
    files = args.paths or sorted((ROOT / "esphome" / "components").rglob("*.cpp"))
    counts: dict[str, int] = {}
    for path in files:
        result = convert(path)
        counts[result] = counts.get(result, 0) + 1
        if result.startswith("skipped"):
            print(f"{path.relative_to(ROOT)}: {result}")
    for result, count in sorted(counts.items()):
        print(f"{result}: {count}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
