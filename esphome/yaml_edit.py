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


class RestoreError(EsphomeError):
    """A rollback left at least one file as it was written."""


def read_text(path: Path) -> str:
    """The file as written, line endings included; read_file would fold them."""
    try:
        return path.read_bytes().decode("utf-8")
    except (OSError, UnicodeDecodeError) as err:
        raise EsphomeError(f"Error reading file {path}: {err}") from err


@dataclass
class LineEdit:
    """One line to rewrite, or with ``insert_after`` a line to add right
    after it; ``old_line`` is what the line held when it was located."""

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


_DOMAIN = "yaml_edit"


def own_documents() -> set[Path]:
    """Every file the loader read for this configuration, found once per run."""
    if (documents := CORE.data.get(_DOMAIN)) is None:
        documents = CORE.data[_DOMAIN] = _own_documents()
    return documents


def _own_documents() -> set[Path]:
    documents = {str(CORE.config_path)}

    def walk(node: object) -> None:
        if (rng := getattr(node, "esp_range", None)) is not None:
            documents.add(rng.start_mark.document)
        if isinstance(node, dict):
            for key, value in node.items():
                walk(key)
                walk(value)
        elif isinstance(node, list):
            for value in node:
                walk(value)

    walk(CORE.raw_config or {})
    # A file that only gave substitutions leaves no node behind; the tracker
    # also lists a secrets file the loader only tried, hence the is_file check
    documents.update(
        map(str, yaml_util.discover_user_yaml_files(CORE.config_path).files)
    )
    return {p.resolve() for d in documents if (p := Path(d)).is_file()}


def source_of(mapping: ConfigType, name: str) -> tuple[Path, int] | None:
    """The file and line ``name:`` was read from; None when validation added
    it or a merge key brought it in from an anchor elsewhere."""
    rng = getattr(next((k for k in mapping if k == name), None), "esp_range", None)
    if rng is None:
        return None
    if (own := getattr(mapping, "esp_range", None)) is None:
        # merge_config keeps the first mapping's keys with the last one's
        # values, so only the value's range names the line that won
        rng = getattr(mapping.get(name), "esp_range", None)
        if rng is None or Path(rng.start_mark.document).resolve() not in (
            own_documents()
        ):
            return None
        return Path(rng.start_mark.document), rng.start_mark.line
    # An included mapping carries its parent's `!include` line, so only a
    # key in the same document can be checked against the mapping's lines
    if (
        rng.start_mark.document == own.start_mark.document
        and not own.start_mark.line <= rng.start_mark.line <= own.end_mark.line
    ):
        return None
    return Path(rng.start_mark.document), rng.start_mark.line


def editable_file(path: Path) -> Path:
    """``path`` resolved, so a symlink's target is edited; refuses a file
    outside the config dir or under the build data."""
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
    """The file, line number and text ``name:`` was read from."""
    if (source := source_of(mapping, name)) is None:
        raise EsphomeError(f"'{name}' was not read from a file")
    doc, line_no = source
    # Returned as the loader saw it: a `!secret` in a symlinked include
    # resolves beside the link
    editable_file(doc)
    return doc, line_no, line_at(doc, line_no)


def write_keeping_mode(path: Path, text: str, like: Path | None = None) -> None:
    """Write with the mode of ``like`` (default: the file itself) rather
    than write_file's 0644; a 0600 secrets file stays 0600."""
    try:
        mode = stat.S_IMODE((like or path).stat().st_mode)
    except OSError as err:
        raise EsphomeError(f"Could not read the mode of {like or path}: {err}") from err
    try:
        write_file(path, text, private=True)
    except EsphomeError as err:
        # write_file keeps the reason in the cause only
        raise EsphomeError(f"{err}: {err.__cause__}") from err
    try:
        path.chmod(mode)
    except OSError as err:
        raise EsphomeError(
            f"{path} was written but could not get its mode back: {err}"
        ) from err


def rewritten_text(original: str, edits: list[LineEdit]) -> str:
    """``original`` with the edits applied; a line that changed is refused."""
    lines = original.splitlines(keepends=True)
    file_newline = "\r\n" if "\r\n" in original else "\n"
    # Bottom up, so an insertion never shifts a later edit
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
    """Rewrite the located lines in place; returns each file's text before
    and after for restore_files. Nothing is written while any line is stale,
    and a file that no longer loads is put back here."""
    by_path: dict[Path, list[LineEdit]] = {}
    for edit in edits:
        by_path.setdefault(editable_file(edit.path), []).append(edit)
    originals = {}
    for path, own in by_path.items():
        original = read_text(path)
        originals[path] = Snapshot(original, rewritten_text(original, own))
    try:
        for path, snapshot in originals.items():
            write_keeping_mode(path, snapshot.written)
        for path in originals:
            try:
                yaml_util.load_yaml(path, track_document_range=False)
            except EsphomeError as err:
                raise EsphomeError(f"{path} no longer loads: {err}") from err
        compiled_config.invalidate_compiled_config()
    except BaseException as err:
        try:
            restore_files(originals)
        except RestoreError as restore_err:
            if not isinstance(err, Exception):
                err.add_note(str(restore_err))  # an interrupt stays one
                raise err from None
            raise RestoreError(f"{err}; {restore_err}") from err
        raise
    return originals


def restore_files(originals: dict[Path, Snapshot]) -> None:
    """Put back what apply_line_edits wrote; a file that failed or changed
    meanwhile is left alone and reported."""
    failed = []
    for path, snapshot in originals.items():
        try:
            current = read_text(path)
            if current == snapshot.original:
                continue  # never written, or already put back
            if current != snapshot.written:
                raise EsphomeError("changed since it was written, left as is")
            write_keeping_mode(path, snapshot.original)
        except EsphomeError as err:
            failed.append(f"{path}: {err}")
    try:
        compiled_config.invalidate_compiled_config()
    except EsphomeError as err:
        failed.append(str(err))
    if failed:
        raise RestoreError("Could not restore " + "; ".join(failed))
