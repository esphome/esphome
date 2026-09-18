"""Rewrite single lines of a yaml file in place, located by the source
ranges the loader keeps, so quotes, comments, indentation and line endings
around them survive and nothing else in the file is touched."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re
import stat

from esphome import compiled_config, yaml_util
from esphome.core import CORE, EsphomeError
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
    """One line to rewrite, or a line to add right after it; ``old_line`` is
    what the line held when it was located."""

    path: Path
    line: int
    old_line: str
    new_line: str
    insert_after: bool = False


@dataclass
class Snapshot:
    """A rewritten file's text before and after; the restore only puts
    ``original`` back over ``written``, never over the user's own edit."""

    original: str
    written: str


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


def editable_file(path: Path) -> Path:
    """``path`` resolved, so a rewrite lands on a symlink's target; refuses a
    file outside the configuration directory or under the build data."""
    resolved = path.resolve()
    if (
        not path.is_file()
        or not resolved.is_relative_to(CORE.config_dir.resolve())
        or resolved.is_relative_to(CORE.data_dir.resolve())
    ):
        raise EsphomeError(
            f"{path} is not an editable file in the configuration directory"
        )
    return resolved


def source_line(mapping: ConfigType, name: str) -> tuple[Path, int, str]:
    """The file, line number and text ``name:`` was read from; refuses a
    source that was not read from an editable file."""
    if (source := source_of(mapping, name)) is None:
        raise EsphomeError(f"'{name}' was not read from a file")
    doc, line_no = source
    doc = editable_file(doc)
    return doc, line_no, line_at(doc, line_no)


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
    file_newline = "\r\n" if "\r\n" in original else "\n"
    # Highest line first, so an insertion never shifts a later edit; an
    # insertion after a line goes before that line's own rewrite
    for edit in sorted(edits, key=lambda e: (e.line, e.insert_after), reverse=True):
        text = lines[edit.line].rstrip("\r\n") if edit.line < len(lines) else None
        if text != edit.old_line:
            raise EsphomeError(f"{edit.path}:{edit.line + 1} changed since it was read")
        ending = lines[edit.line][len(text) :]
        if edit.insert_after:
            # A last line without a newline gets the file's own kind
            lines[edit.line] = text + (ending or file_newline)
            lines.insert(edit.line + 1, edit.new_line + ending)
        else:
            lines[edit.line] = edit.new_line + ending
    return "".join(lines)


def apply_line_edits(edits: list[LineEdit]) -> dict[Path, Snapshot]:
    """Rewrite the located lines in place and return each touched file's
    text before and after, for a rollback. Every file is rewritten in
    memory before any is written, so a stale line touches nothing; a file
    that no longer loads is undone here."""
    by_path: dict[Path, list[LineEdit]] = {}
    for edit in edits:
        # Resolved here as well, so a hand-built edit cannot reach a symlink
        # itself or a file outside the configuration directory
        by_path.setdefault(editable_file(edit.path), []).append(edit)
    snapshots = {}
    for path, own in by_path.items():
        original = read_text(path)
        snapshots[path] = Snapshot(original, rewritten_text(original, own))
    try:
        for path, snapshot in snapshots.items():
            write_keeping_mode(path, snapshot.written)
        for path in snapshots:
            try:
                yaml_util.load_yaml(path, track_document_range=False)
            except EsphomeError as err:
                raise EsphomeError(f"{path} no longer loads: {err}") from err
        compiled_config.invalidate_compiled_config()
    except BaseException as err:
        try:
            restore_files(snapshots)
        except EsphomeError as restore_err:
            raise EsphomeError(f"{err}; {restore_err}") from err
        raise
    return snapshots


def restore_files(snapshots: dict[Path, Snapshot]) -> None:
    """Put back the files apply_line_edits rewrote; every file is tried and
    the ones that failed, or that the user changed meanwhile, are reported
    together and left alone."""
    failed = []
    for path, snapshot in snapshots.items():
        try:
            if read_text(path) != snapshot.written:
                raise EsphomeError("changed since it was written, left as is")
            write_keeping_mode(path, snapshot.original)
        except EsphomeError as err:
            failed.append(f"{path}: {err}")
    try:
        compiled_config.invalidate_compiled_config()
    except EsphomeError as err:
        failed.append(str(err))
    if failed:
        raise EsphomeError("Could not restore " + "; ".join(failed))
