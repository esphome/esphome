"""Rewrite the OTA encryption key in place: the `key:` line the loader read
it from, or the secrets.yaml line a `!secret` on it points to."""

from __future__ import annotations

from dataclasses import dataclass, field
import logging
from pathlib import Path
import re
import stat

from esphome.components.noise import static_encryption_key
from esphome.const import (
    CONF_API,
    CONF_ENCRYPTION,
    CONF_ESPHOME,
    CONF_KEY,
    CONF_OTA,
    CONF_PLATFORM,
)
from esphome.core import CORE, EsphomeError
from esphome.espota2 import CONF_OLD_KEY
from esphome.helpers import write_file
from esphome.types import ConfigType
from esphome.yaml_util import secrets_path_for

_LOGGER = logging.getLogger(__name__)


def _read_text(path: Path) -> str:
    """The file as written, line endings included; read_file would fold them."""
    try:
        return path.read_bytes().decode("utf-8")
    except (OSError, UnicodeDecodeError) as err:
        raise EsphomeError(f"Error reading file {path}: {err}") from err


@dataclass
class KeyEdit:
    """One yaml line to rewrite, or a line to add right after it; ``old_line``
    is what the line held when it was located."""

    path: Path
    line: int
    old_line: str
    new_line: str
    insert_after: bool = False
    # Other configurations that use the rewritten secret
    shared_with: list[Path] = field(default_factory=list)


def _esphome_ota_items(raw: ConfigType) -> list[ConfigType]:
    """The esphome ota entries; a raw ``ota:`` may be a mapping, not a list,
    and a package/device split may contribute several same-port entries."""
    ota = raw.get(CONF_OTA) or []
    items = [ota] if isinstance(ota, dict) else ota
    return [
        item
        for item in items
        if isinstance(item, dict) and item.get(CONF_PLATFORM) == CONF_ESPHOME
    ]


def _key_blocks(raw: ConfigType, key: str) -> list[ConfigType]:
    """The api and esphome ota ``encryption:`` mappings whose key is ``key``."""
    blocks = [raw.get(CONF_API) or {}, *_esphome_ota_items(raw)]
    # Validation writes the inherited api key into a bare ota block; only a
    # `key:` that was read from a file has a range
    return [
        block[CONF_ENCRYPTION]
        for block in blocks
        if (node := static_encryption_key(block)) is not None
        and str(node) == key
        and _source_of(block[CONF_ENCRYPTION], CONF_KEY) is not None
    ]


def _line_at(doc: Path, line_no: int) -> str:
    lines = _read_text(doc).splitlines()
    return lines[line_no] if line_no < len(lines) else ""


_INDENT_RE = re.compile(r"\s*")
# A plain scalar: optional matching quotes, then the line's trailing comment
_PLAIN_SCALAR = r"[^\s#\"']*"
_TRAILER = r"(?P<quote>[\"']?){value}(?P=quote)(?P<trail>\s*(?:#.*)?)$"
# `!secret name`, the name optionally quoted
_SECRET_REF = r"!secret\s+(?P<q>[\"']?)(?P<name>[^\s#\"']+)(?P=q)"
_KEY_SECRET_RE = re.compile(rf"^\s*{CONF_KEY}\s*:\s*{_SECRET_REF}")
_OLD_KEY_SECRET_RE = re.compile(rf"^\s*{CONF_OLD_KEY}\s*:\s*{_SECRET_REF}")
_KEY_FIELD_RE = re.compile(rf"^\s*{CONF_KEY}\s*:")
_BARE_BLOCK_RE = re.compile(rf"^\s*{CONF_ENCRYPTION}\s*:\s*(#.*)?$")
_YAML_SUFFIXES = (".yaml", ".yml")


def _indent(text: str) -> str:
    return _INDENT_RE.match(text).group()


def _scalar_line_re(prefix: str, value: str | None = None) -> re.Pattern[str]:
    """Match ``prefix`` followed by ``value``, or by any plain scalar, keeping
    the prefix, the quotes and the trailer for _rewrite."""
    scalar = _PLAIN_SCALAR if value is None else re.escape(value)
    return re.compile(rf"^(?P<prefix>{prefix}){_TRAILER.format(value=scalar)}")


def _field_line_re(field: str, value: str) -> re.Pattern[str]:
    """``field: value`` at any indent."""
    return _scalar_line_re(rf"\s*{field}\s*:\s*", value)


def _secret_line_re(indent: str, name: str, value: str | None) -> re.Pattern[str]:
    """The ``name:`` line of a secrets file whose root sits at ``indent``."""
    return _scalar_line_re(
        rf"{re.escape(indent)}[\"']?{re.escape(name)}[\"']?\s*:\s*", value
    )


def _secret_use_re(name: str) -> re.Pattern[str]:
    return re.compile(rf"!secret\s+[\"']?{re.escape(name)}[\"']?(?=[\s#]|$)")


def _file_use_re(file_name: str) -> re.Pattern[str]:
    return re.compile(re.escape(file_name))


def _rewrite(match: re.Match[str], value: str) -> str:
    quote = match["quote"]
    return f"{match['prefix']}{quote}{value}{quote}{match['trail']}"


def _root_indent(lines: list[str]) -> str:
    """The indent of the first content line: a secrets file may indent its
    whole root mapping."""
    content = (
        t for t in lines if t.strip() and not t.strip().startswith(("#", "---", "%"))
    )
    return next((_indent(t) for t in content), "")


def _source_of(mapping: ConfigType, name: str) -> tuple[Path, int] | None:
    """The file and line ``name:`` was read from, None when validation added
    it. Mapping keys keep their range through validation, values may not."""
    rng = getattr(next((k for k in mapping if k == name), None), "esp_range", None)
    if rng is None:
        return None
    return Path(rng.start_mark.document), rng.start_mark.line


def _editable_file(path: Path) -> Path:
    """``path`` resolved, so a rewrite lands on a symlink's target; refuses a
    file outside the configuration directory or under the build data."""
    resolved = path.resolve()
    if (
        not path.is_file()
        or not resolved.is_relative_to(CORE.config_dir.resolve())
        or resolved.is_relative_to(CORE.data_dir.resolve())
    ):
        raise EsphomeError(
            f"The key comes from {path}, which is not an editable file in the "
            "configuration directory"
        )
    return resolved


def _editable_source(mapping: ConfigType, name: str) -> tuple[Path, int]:
    """The file and line ``name:`` was read from; refuses uneditable sources."""
    if (source := _source_of(mapping, name)) is None:
        raise EsphomeError(f"'{name}' was not read from a file")
    doc, line_no = source
    return _editable_file(doc), line_no


def _write_keeping_mode(path: Path, text: str) -> None:
    """secrets.yaml is often 0600; write_file would widen it to 0644."""
    try:
        mode = stat.S_IMODE(path.stat().st_mode)
    except OSError as err:
        raise EsphomeError(f"Could not read {path}: {err}") from err
    write_file(path, text, private=True)
    try:
        path.chmod(mode)
    except OSError as err:
        raise EsphomeError(f"Could not keep the mode of {path}: {err}") from err


def _files_referencing(ref: re.Pattern[str], own: set[Path]) -> list[Path]:
    """Yaml files in the configuration directory, outside ``own`` and the
    build data, whose text matches ``ref``; the scan only feeds a warning,
    so a file that cannot be read is skipped."""
    data_dir = CORE.data_dir.resolve()
    hits = []
    for path in CORE.config_dir.resolve().rglob("*"):
        if path.suffix not in _YAML_SUFFIXES or path in own:
            continue
        if path.is_relative_to(data_dir) or not path.is_file():
            continue
        try:
            text = _read_text(path)
        except EsphomeError as err:
            _LOGGER.debug("Skipping %s: %s", path, err)
            continue
        if ref.search(text):
            hits.append(path)
    return sorted(hits)


def _secret_line(
    secrets_path: Path, name: str, value: str | None = None
) -> tuple[int, re.Match[str]] | None:
    """The root level ``name:`` line, None when absent; the loader already
    refused a duplicate key."""
    lines = _read_text(secrets_path).splitlines()
    line_re = _secret_line_re(_root_indent(lines), name, value)
    return next(
        ((i, m) for i, text in enumerate(lines) if (m := line_re.match(text))), None
    )


def _secret_edit(
    doc: Path, name: str, old_key: str, new_key: str, own: set[Path]
) -> KeyEdit:
    """The secrets.yaml line ``!secret name`` in ``doc`` resolves to; notes
    the other configurations that share it, since their devices keep the
    previous key and get no old_key."""
    secrets_path = _editable_file(secrets_path_for(doc))
    if (hit := _secret_line(secrets_path, name, old_key)) is None:
        raise EsphomeError(f"No '{name}:' line with the current key in {secrets_path}")
    i, m = hit
    return KeyEdit(
        secrets_path,
        i,
        m.string,
        _rewrite(m, new_key),
        shared_with=_files_referencing(_secret_use_re(name), own),
    )


def _old_secret_edit(
    doc: Path, name: str, old_key: str, after: str | None = None
) -> KeyEdit:
    """Keep the previous key in the secrets file as ``name``: rewritten in
    place, or added under the ``after`` line it came from. One of the two
    lines exists, the loader resolved it."""
    secrets_path = _editable_file(secrets_path_for(doc))
    if (hit := _secret_line(secrets_path, name)) is not None:
        i, m = hit
        return KeyEdit(secrets_path, i, m.string, _rewrite(m, old_key))
    i, m = _secret_line(secrets_path, after)
    return KeyEdit(
        secrets_path,
        i,
        m.string,
        f'{_indent(m.string)}{name}: "{old_key}"',
        insert_after=True,
    )


def locate_key_edits(old_key: str, new_key: str) -> list[KeyEdit]:
    """The edits that replace the current key; raises EsphomeError for a
    substitution, flow mapping or remote package."""
    literal_re = _field_line_re(CONF_KEY, old_key)
    blocks = _key_blocks(CORE.raw_config or {}, old_key)
    if not blocks:
        raise EsphomeError("The current key was not found on a line of the yaml")
    sources = [_editable_source(block, CONF_KEY) for block in blocks]
    main = CORE.config_path.resolve()
    own = {doc for doc, _ in sources} | {main}
    edits: list[KeyEdit] = []
    for doc, line_no in sources:
        text = _line_at(doc, line_no)
        if match := _KEY_SECRET_RE.match(text):
            edit = _secret_edit(doc, match["name"], old_key, new_key, own)
        elif match := literal_re.match(text):
            edit = KeyEdit(doc, line_no, text, _rewrite(match, new_key))
            if doc != main:
                # An include or local package may serve other configurations
                edit.shared_with = _files_referencing(_file_use_re(doc.name), own)
        else:
            raise EsphomeError(
                f"{doc}:{line_no + 1} does not hold the key as a plain value or "
                "a !secret; edit the key by hand"
            )
        # The api and ota blocks may share one !secret line
        if edit not in edits:
            edits.append(edit)
    return edits


def _key_secret(old_key: str) -> tuple[Path, str] | None:
    """The file and name of a ``key: !secret name`` line holding the key."""
    for block in _key_blocks(CORE.raw_config or {}, old_key):
        doc, line_no = _editable_source(block, CONF_KEY)
        if match := _KEY_SECRET_RE.match(_line_at(doc, line_no)):
            return doc, match["name"]
    return None


def old_key_edit(old_key: str) -> list[KeyEdit]:
    """Set ``old_key:`` on the esphome ota block, rewriting or adding it. A
    key kept in secrets.yaml stays there: ``old_key: !secret <name>_old``
    and a second edit for that line.

    With several same-port entries the one that carries ``old_key`` or
    ``key`` in its own lines gets it, else the first with a block.
    """
    items = [
        item
        for item in _esphome_ota_items(CORE.raw_config or {})
        if CONF_ENCRYPTION in item
    ]
    if not items:
        raise EsphomeError("The esphome OTA platform has no 'encryption:' block")
    item = next(
        (
            item
            for item in items
            for name in (CONF_OLD_KEY, CONF_KEY)
            if _source_of(item[CONF_ENCRYPTION] or {}, name) is not None
        ),
        items[0],
    )
    encryption = item[CONF_ENCRYPTION] or {}
    if (existing := encryption.get(CONF_OLD_KEY)) is not None:
        doc, line_no = _editable_source(encryption, CONF_OLD_KEY)
        text = _line_at(doc, line_no)
        if match := _OLD_KEY_SECRET_RE.match(text):
            return [_old_secret_edit(doc, match["name"], old_key)]
        match = _field_line_re(CONF_OLD_KEY, str(existing)).match(text)
        if match is None:
            raise EsphomeError(
                f"{doc}:{line_no + 1} does not hold '{CONF_OLD_KEY}' as a plain "
                "value; edit it by hand"
            )
        return [KeyEdit(doc, line_no, text, _rewrite(match, old_key))]
    if (secret := _key_secret(old_key)) is None:
        rendered = f'{CONF_OLD_KEY}: "{old_key}"'
        extra = []
    else:
        secret_doc, name = secret
        rendered = f"{CONF_OLD_KEY}: !secret {name}_old"
        extra = [_old_secret_edit(secret_doc, f"{name}_old", old_key, after=name)]
    # The block's own key line sets the indent, in whichever file holds it
    if _source_of(encryption, CONF_KEY) is not None:
        doc, line_no = _editable_source(encryption, CONF_KEY)
        text = _line_at(doc, line_no)
        if not _KEY_FIELD_RE.match(text):
            raise EsphomeError(
                f"{doc}:{line_no + 1} does not hold '{CONF_KEY}'; edit it by hand"
            )
        edit = KeyEdit(doc, line_no, text, _indent(text) + rendered, insert_after=True)
        return [edit, *extra]
    # A bare block indents one level below the `encryption:` key
    doc, line_no = _editable_source(item, CONF_ENCRYPTION)
    text = _line_at(doc, line_no)
    if not _BARE_BLOCK_RE.match(text):
        raise EsphomeError(
            f"{doc}:{line_no + 1} writes the block in flow style; edit it by hand"
        )
    edit = KeyEdit(
        doc, line_no, text, f"{_indent(text)}  {rendered}", insert_after=True
    )
    return [edit, *extra]


def apply_key_edits(edits: list[KeyEdit]) -> dict[Path, str]:
    """Rewrite the located lines and return each touched file's original
    text for a rollback; a file that no longer loads is undone here."""
    from esphome import yaml_util
    from esphome.compiled_config import invalidate_compiled_config

    originals: dict[Path, str] = {}
    current: dict[Path, list[str]] = {}
    newline: dict[Path, str] = {}
    try:
        # Highest line first, so an insertion never shifts a later edit; an
        # insertion after a line goes before that line's own rewrite
        for edit in sorted(edits, key=lambda e: (e.line, e.insert_after), reverse=True):
            if edit.path not in originals:
                originals[edit.path] = _read_text(edit.path)
                current[edit.path] = originals[edit.path].splitlines(keepends=True)
                newline[edit.path] = "\r\n" if "\r\n" in originals[edit.path] else "\n"
            lines = current[edit.path]
            nl = newline[edit.path]
            text = lines[edit.line].rstrip("\r\n") if edit.line < len(lines) else None
            if text != edit.old_line:
                raise EsphomeError(
                    f"{edit.path}:{edit.line + 1} changed since it was read"
                )
            ending = lines[edit.line][len(text) :]
            if edit.insert_after:
                lines[edit.line] = text + nl
                lines.insert(edit.line + 1, edit.new_line + ending)
            else:
                lines[edit.line] = edit.new_line + ending
        for path, lines in current.items():
            _write_keeping_mode(path, "".join(lines))
        for path in originals:
            try:
                yaml_util.load_yaml(path, track_document_range=False)
            except EsphomeError as err:
                raise EsphomeError(f"{path} no longer loads: {err}") from err
        invalidate_compiled_config()
    except BaseException as err:
        try:
            restore_key_files(originals)
        except EsphomeError as restore_err:
            raise EsphomeError(f"{err}; {restore_err}") from err
        raise
    return originals


def restore_key_files(originals: dict[Path, str]) -> None:
    """Put back the files apply_key_edits rewrote; every file is tried and
    the ones that failed are reported together."""
    from esphome.compiled_config import invalidate_compiled_config

    failed = []
    for path, text in originals.items():
        try:
            _write_keeping_mode(path, text)
        except EsphomeError as err:
            failed.append(f"{path}: {err}")
    try:
        invalidate_compiled_config()
    except EsphomeError as err:
        failed.append(str(err))
    if failed:
        raise EsphomeError("Could not restore " + "; ".join(failed))
