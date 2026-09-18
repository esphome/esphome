"""Rewrite single lines of a yaml file in place, located by the source
ranges the loader keeps, so quotes, comments, indentation and line endings
around them survive and nothing else in the file is touched."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re
import stat

from esphome.core import CORE, EsphomeError
from esphome.helpers import write_file
from esphome.types import ConfigType
from esphome.yaml_util import SECRET_YAML

INDENT_RE = re.compile(r"\s*")
# A plain scalar with no yaml indicator, so a block scalar (`>-`, `|`) or a
# flow collection never counts; then optional matching quotes and the trailer
PLAIN_SCALAR = r"[^\s#\"'>|&*!%@`\[\]{},]*"
TRAILER = r"(?P<quote>[\"']?){value}(?P=quote)(?P<trail>\s*(?:#.*)?)$"


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
    written: str | None = None

    def __post_init__(self) -> None:
        if self.written is None:
            self.written = self.original


def indent(text: str) -> str:
    return INDENT_RE.match(text).group()


def scalar_line_re(prefix: str, value: str | None = None) -> re.Pattern[str]:
    """Match ``prefix`` followed by ``value``, or by any plain scalar, keeping
    the prefix, the quotes and the trailer for rewrite."""
    scalar = PLAIN_SCALAR if value is None else re.escape(value)
    return re.compile(rf"^(?P<prefix>{prefix}){TRAILER.format(value=scalar)}")


def field_line_re(name: str, value: str | None = None) -> re.Pattern[str]:
    """``name: value`` at any indent."""
    return scalar_line_re(rf"\s*{re.escape(name)}\s*:\s*", value)


def rewrite(match: re.Match[str], value: str, quote: str | None = None) -> str:
    """The matched line with ``value`` in place of the scalar; the source
    quotes stay unless ``quote`` is given."""
    quote = match["quote"] if quote is None else quote
    return f"{match['prefix']}{quote}{value}{quote}{match['trail']}"


def root_indent(lines: list[str]) -> str:
    """The indent of the first content line: a secrets file may indent its
    whole root mapping."""
    content = (
        t for t in lines if t.strip() and not t.strip().startswith(("#", "---", "%"))
    )
    return next((indent(t) for t in content), "")


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


def secrets_path_for(document: Path) -> Path:
    """The secrets.yaml a ``!secret`` in ``document`` resolves against: the
    one beside the document when it loads, else the main config's, the
    way the loader falls back."""
    from esphome import yaml_util

    beside = document.parent / SECRET_YAML
    if document == CORE.config_path:
        return beside
    try:
        yaml_util.load_yaml(beside, clear_secrets=False, track_document_range=False)
    except EsphomeError:
        return CORE.config_path.parent / SECRET_YAML
    return beside


def secret_line_re(indent_: str, name: str, value: str | None) -> re.Pattern[str]:
    """The ``name:`` line of a secrets file whose root sits at ``indent_``."""
    return scalar_line_re(
        rf"{re.escape(indent_)}[\"']?{re.escape(name)}[\"']?\s*:\s*", value
    )


def secret_line(
    secrets_path: Path, name: str, value: str | None = None
) -> tuple[int, re.Match[str]] | None:
    """The root level ``name:`` line, None when absent; the loader already
    refused a duplicate key."""
    lines = read_text(secrets_path).splitlines()
    line_re = secret_line_re(root_indent(lines), name, value)
    return next(
        ((i, m) for i, text in enumerate(lines) if (m := line_re.match(text))), None
    )


def secret_rewrite(
    secrets_path: Path, name: str, value: str, expect: str | None = None
) -> LineEdit | None:
    """Set the root level ``name:`` line to ``value``; None when the line
    is not a plain scalar, or with ``expect``, does not hold it."""
    if (hit := secret_line(secrets_path, name, expect)) is None:
        return None
    i, m = hit
    return LineEdit(secrets_path, i, m.string, rewrite(m, value))


def secret_insert(secrets_path: Path, after: str, name: str, value: str) -> LineEdit:
    """Add ``name: value`` under the root level ``after`` line."""
    if (hit := secret_line(secrets_path, after)) is None:
        raise EsphomeError(
            f"No plain '{after}:' line in {secrets_path}; edit it by hand"
        )
    i, m = hit
    return LineEdit(
        secrets_path,
        i,
        m.string,
        f'{indent(m.string)}{name}: "{value}"',
        insert_after=True,
    )


def write_keeping_mode(path: Path, text: str) -> None:
    """secrets.yaml is often 0600; write_file would widen it to 0644."""
    try:
        mode = stat.S_IMODE(path.stat().st_mode)
        write_file(path, text, private=True)
        path.chmod(mode)
    except OSError as err:
        raise EsphomeError(f"Could not keep the mode of {path}: {err}") from err


def rewritten_text(original: str, edits: list[LineEdit]) -> str:
    """``original`` with the edits applied; every edit must still find the
    line it was located on."""
    lines = original.splitlines(keepends=True)
    # Highest line first, so an insertion never shifts a later edit; an
    # insertion after a line goes before that line's own rewrite
    for edit in sorted(edits, key=lambda e: (e.line, e.insert_after), reverse=True):
        text = lines[edit.line].rstrip("\r\n") if edit.line < len(lines) else None
        if text != edit.old_line:
            raise EsphomeError(f"{edit.path}:{edit.line + 1} changed since it was read")
        ending = lines[edit.line][len(text) :]
        if edit.insert_after:
            # A last line without a newline gets the file's own kind
            nl = ending or ("\r\n" if "\r\n" in original else "\n")
            lines[edit.line] = text + nl
            lines.insert(edit.line + 1, edit.new_line + ending)
        else:
            lines[edit.line] = edit.new_line + ending
    return "".join(lines)


def apply_line_edits(edits: list[LineEdit]) -> dict[Path, Snapshot]:
    """Rewrite the located lines in place and return each touched file's
    text before and after, for a rollback; a file that no longer loads is
    undone here."""
    from esphome import yaml_util
    from esphome.compiled_config import invalidate_compiled_config

    snapshots: dict[Path, Snapshot] = {}
    try:
        for path in {edit.path for edit in edits}:
            snapshots[path] = Snapshot(read_text(path))
        for path, snapshot in snapshots.items():
            snapshot.written = rewritten_text(
                snapshot.original, [e for e in edits if e.path == path]
            )
            write_keeping_mode(path, snapshot.written)
        for path in snapshots:
            try:
                yaml_util.load_yaml(path, track_document_range=False)
            except EsphomeError as err:
                raise EsphomeError(f"{path} no longer loads: {err}") from err
        invalidate_compiled_config()
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
    from esphome.compiled_config import invalidate_compiled_config

    failed = []
    for path, snapshot in snapshots.items():
        try:
            if read_text(path) != snapshot.written:
                raise EsphomeError("changed since it was written, left as is")
            write_keeping_mode(path, snapshot.original)
        except EsphomeError as err:
            failed.append(f"{path}: {err}")
    try:
        invalidate_compiled_config()
    except EsphomeError as err:
        failed.append(str(err))
    if failed:
        raise EsphomeError("Could not restore " + "; ".join(failed))
