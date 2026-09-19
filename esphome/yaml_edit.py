"""Rewrite single lines of a yaml file in place, located by the source
ranges the loader keeps, so quotes, comments, indentation and line endings
around them survive and nothing else in the file is touched."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re
import stat

from esphome.core import EsphomeError
from esphome.helpers import write_file
from esphome.types import ConfigType

# A plain scalar with no yaml indicator, so an empty value, a block scalar
# (`>-`, `|`) or a flow collection never counts; then optional matching
# quotes and the trailer, where a comment needs whitespace before its `#`
PLAIN_SCALAR = r"[^\s#\"'>|&*!%@`\[\]{},]+"
TRAILER = r"(?P<quote>[\"']?){value}(?P=quote)(?P<trail>(?:\s+#.*)?\s*)$"


def read_text(path: Path) -> str:
    """The file as written, line endings included; read_file would fold them."""
    try:
        return path.read_bytes().decode("utf-8")
    except (OSError, UnicodeDecodeError) as err:
        raise EsphomeError(f"Error reading file {path}: {err}") from err


@dataclass
class LineEdit:
    """One line to rewrite; ``old_line`` is what it held when located."""

    path: Path
    line: int
    old_line: str
    new_line: str


def field_line_re(
    name: str, value: str | None = None, indent: str = r"\s*"
) -> re.Pattern[str]:
    """Match ``name: value``, or ``name:`` with any plain scalar, at
    ``indent``; the name may be quoted. Keeps the prefix, the quotes and the
    trailer for rewrite."""
    scalar = PLAIN_SCALAR if value is None else re.escape(value)
    prefix = rf"{indent}[\"']?{re.escape(name)}[\"']?\s*:\s*"
    return re.compile(rf"^(?P<prefix>{prefix}){TRAILER.format(value=scalar)}")


def rewrite(match: re.Match[str], value: str, quote: str | None = None) -> str:
    """The matched line with ``value`` in place of the scalar; the source
    quotes stay unless ``quote`` is given."""
    quote = match["quote"] if quote is None else quote
    return f"{match['prefix']}{quote}{value}{quote}{match['trail']}"


def line_at(doc: Path, line_no: int) -> str:
    lines = read_text(doc).splitlines()
    if line_no >= len(lines):
        raise EsphomeError(f"{doc}:{line_no + 1} changed since it was read")
    return lines[line_no]


def source_of(mapping: ConfigType, name: str) -> tuple[Path, int] | None:
    """The file and line ``name:`` was read from, None when validation added
    it. Mapping keys keep their range through validation, values may not."""
    rng = getattr(next((k for k in mapping if k == name), None), "esp_range", None)
    if rng is None:
        return None
    return Path(rng.start_mark.document), rng.start_mark.line


def write_keeping_mode(path: Path, text: str, like: Path | None = None) -> None:
    """Write with the mode of ``like`` (default: the file itself) rather
    than write_file's 0644; a 0600 secrets file stays 0600."""
    try:
        mode = stat.S_IMODE((like or path).stat().st_mode)
        write_file(path, text, private=True)
        path.chmod(mode)
    except OSError as err:
        raise EsphomeError(f"Could not keep the mode of {path}: {err}") from err


def rewritten_text(original: str, edits: list[LineEdit]) -> str:
    """``original`` with the edits applied; every edit must still find the
    line it was located on."""
    lines = original.splitlines(keepends=True)
    for edit in edits:
        text = lines[edit.line].rstrip("\r\n") if edit.line < len(lines) else None
        if text != edit.old_line:
            raise EsphomeError(f"{edit.path}:{edit.line + 1} changed since it was read")
        lines[edit.line] = edit.new_line + lines[edit.line][len(text) :]
    return "".join(lines)
