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
    """A rewritten file's text before and after, and its mode before; the
    restore only puts ``original`` back over ``written``, never over the
    user's own edit."""

    original: str
    written: str
    mode: int | None = None


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
    """Every file the loader read for this configuration, found once per run.
    A file behind an include path with a substitution that gave nothing but
    substitutions is the one kind this misses."""
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
    found = yaml_util.discover_user_yaml_files(CORE.config_path, clear_secrets=False)
    if found.load_errors:  # an incomplete set would misplace a key
        raise EsphomeError(
            "Could not read every file of the configuration: "
            + "; ".join(found.load_errors)
        )
    documents.update(map(str, found.files))
    return {p.resolve() for d in documents if (p := Path(d)).is_file()}


def source_of(mapping: ConfigType, name: str) -> tuple[Path, int] | None:
    """The file and line ``name:`` was read from; None when validation added
    it or a merge key brought it in from an anchor elsewhere."""
    rng = getattr(next((k for k in mapping if k == name), None), "esp_range", None)
    if rng is None:
        return None
    if (own := getattr(mapping, "esp_range", None)) is None:
        # merge_config keeps the first mapping's keys with the last one's
        # values, so only the value's range names the line that won, and
        # only when the key is on it (a block value starts on a line of its own)
        rng = getattr(mapping.get(name), "esp_range", None)
        if rng is None:
            return None
        doc, line = Path(rng.start_mark.document), rng.start_mark.line
        if doc.resolve() not in own_documents() or not field_line_re(name).match(
            line_at(doc, line)
        ):
            return None
        return doc, line
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


def mode_of(path: Path) -> int:
    try:
        return stat.S_IMODE(path.stat().st_mode)
    except OSError as err:
        raise EsphomeError(f"Could not read the mode of {path}: {err}") from err


def write_keeping_mode(
    path: Path, text: str, like: Path | None = None, mode: int | None = None
) -> None:
    """Write with ``mode``, else the mode of ``like`` (default: the file
    itself), rather than write_file's 0644; a 0600 secrets file stays 0600."""
    if mode is None:
        mode = mode_of(like or path)
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
    # Bottom up, so an insertion never shifts a later edit; insertions under
    # one line keep their order
    order = sorted(
        enumerate(edits), key=lambda ie: (ie[1].line, ie[1].insert_after, ie[0])
    )
    for _, edit in reversed(order):
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
    seen_as: dict[Path, Path] = {}  # the path the loader read a file by
    for edit in edits:
        resolved = editable_file(edit.path)
        by_path.setdefault(resolved, []).append(edit)
        seen_as.setdefault(resolved, edit.path)
    originals = {}
    for path, own in by_path.items():
        original = read_text(path)
        originals[path] = Snapshot(
            original, rewritten_text(original, own), mode_of(path)
        )
    try:
        for path, snapshot in originals.items():
            write_keeping_mode(path, snapshot.written)
        # Reloaded as the loader saw them: a relative !include inside a
        # symlinked file resolves beside the link
        reloads = [seen_as[path] for path in originals]
        if CORE.config_path.resolve() not in originals:
            # Its !secret tags register a value written elsewhere
            reloads.append(CORE.config_path)
        for path in reloads:
            try:
                # The secret registry stays: later output still redacts
                yaml_util.load_yaml(
                    path, clear_secrets=False, track_document_range=False
                )
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
    """Put back what apply_line_edits wrote, mode included; a file that
    failed or changed meanwhile is left alone and reported. Every file is
    tried whatever went wrong before; an interrupt or exit is raised again
    at the end with the report as a note."""
    failed: list[str] = []
    stop: BaseException | None = None

    def report(what: str, err: BaseException) -> None:
        nonlocal stop
        if isinstance(err, Exception):
            why = str(err) or type(err).__name__
        else:  # an interrupt or exit says what it is, not its code
            why = (
                "interrupted"
                if isinstance(err, KeyboardInterrupt)
                else type(err).__name__
            )
            stop = err
        failed.append(f"{what}: {why}")

    for path, snapshot in originals.items():
        try:
            current = read_text(path)
            if current == snapshot.original:
                continue  # never written, or already put back
            if current != snapshot.written:
                raise EsphomeError("changed since it was written, left as is")
            write_keeping_mode(path, snapshot.original, mode=snapshot.mode)
        except BaseException as err:  # noqa: BLE001  # pylint: disable=broad-except
            report(str(path), err)
    try:
        compiled_config.invalidate_compiled_config()
    except BaseException as err:  # noqa: BLE001  # pylint: disable=broad-except
        report("the validated caches", err)
    if not failed:
        return
    message = "Could not restore " + "; ".join(failed)
    if stop is not None:
        stop.add_note(message)
        raise stop
    raise RestoreError(message)
