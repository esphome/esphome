"""Rewrite the OTA encryption key in place: the `key:` line the loader read
it from, or the secrets.yaml line a `!secret` on it points to."""

from __future__ import annotations

from dataclasses import dataclass, field
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


def _key_line_re(prefix: str, key: str) -> re.Pattern[str]:
    """Match a key line, capturing what surrounds the key."""
    return re.compile(rf"^({prefix})([\"']?){re.escape(key)}\2(\s*(?:#.*)?)$")


def _rewrite(match: re.Match[str], new_key: str) -> str:
    return f"{match.group(1)}{match.group(2)}{new_key}{match.group(2)}{match.group(3)}"


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


def _other_users(name: str, own: set[Path]) -> list[Path]:
    """Yaml files in the configuration directory, outside ``own`` and the
    build data, that reference ``!secret name``."""
    ref = re.compile(rf"!secret\s+[\"']?{re.escape(name)}[\"']?(?=[\s#]|$)")
    data_dir = CORE.data_dir.resolve()
    return sorted(
        path
        for path in CORE.config_dir.resolve().rglob("*.yaml")
        if path not in own
        and not path.is_relative_to(data_dir)
        and ref.search(_read_text(path))
    )


def _secret_edit(
    doc: Path, name: str, old_key: str, new_key: str, own: set[Path]
) -> KeyEdit:
    """The secrets.yaml line ``!secret name`` in ``doc`` resolves to; notes
    the other configurations that share it, since their devices keep the
    previous key and get no old_key."""
    secrets_path = _editable_file(secrets_path_for(doc))
    # The name may be quoted; nested lines are indented and never match
    line_re = _key_line_re(rf"[\"']?{re.escape(name)}[\"']?:\s*", old_key)
    hits = [
        (i, m)
        for i, text in enumerate(_read_text(secrets_path).splitlines())
        if (m := line_re.match(text))
    ]
    if len(hits) != 1:
        raise EsphomeError(
            f"Expected exactly one '{name}:' line with the current key in "
            f"{secrets_path}, found {len(hits)}"
        )
    i, m = hits[0]
    return KeyEdit(
        secrets_path,
        i,
        m.string,
        _rewrite(m, new_key),
        shared_with=_other_users(name, own),
    )


def locate_key_edits(old_key: str, new_key: str) -> list[KeyEdit]:
    """The edits that replace the current key; raises EsphomeError for a
    substitution, flow mapping or remote package."""
    literal_re = _key_line_re(rf"\s*{CONF_KEY}:\s*", old_key)
    secret_re = re.compile(rf"^\s*{CONF_KEY}:\s*!secret\s+([\"']?)([^\s#\"']+)\1")
    blocks = _key_blocks(CORE.raw_config or {}, old_key)
    if not blocks:
        raise EsphomeError("The current key was not found on a line of the yaml")
    sources = [_editable_source(block, CONF_KEY) for block in blocks]
    own = {doc for doc, _ in sources} | {CORE.config_path.resolve()}
    edits: list[KeyEdit] = []
    for doc, line_no in sources:
        lines = _read_text(doc).splitlines()
        text = lines[line_no] if line_no < len(lines) else ""
        if match := secret_re.match(text):
            edit = _secret_edit(doc, match.group(2), old_key, new_key, own)
        elif match := literal_re.match(text):
            edit = KeyEdit(doc, line_no, text, _rewrite(match, new_key))
        else:
            raise EsphomeError(
                f"{doc}:{line_no + 1} does not hold the key as a plain value or "
                "a !secret; edit the key by hand"
            )
        # The api and ota blocks may share one !secret line
        if edit not in edits:
            edits.append(edit)
    return edits


def old_key_edit(old_key: str) -> KeyEdit:
    """Set ``old_key:`` on the esphome ota block, rewriting or adding it.

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
    rendered = f'{CONF_OLD_KEY}: "{old_key}"'
    if (existing := encryption.get(CONF_OLD_KEY)) is not None:
        doc, line_no = _editable_source(encryption, CONF_OLD_KEY)
        lines = _read_text(doc).splitlines()
        text = lines[line_no] if line_no < len(lines) else ""
        match = _key_line_re(rf"\s*{CONF_OLD_KEY}:\s*", str(existing)).match(text)
        if match is None:
            raise EsphomeError(
                f"{doc}:{line_no + 1} does not hold '{CONF_OLD_KEY}' as a plain "
                "value; edit it by hand"
            )
        return KeyEdit(doc, line_no, text, _rewrite(match, old_key))
    # The block's own key line sets the indent, in whichever file holds it
    if _source_of(encryption, CONF_KEY) is not None:
        doc, line_no = _editable_source(encryption, CONF_KEY)
        lines = _read_text(doc).splitlines()
        text = lines[line_no] if line_no < len(lines) else ""
        if not re.match(rf"\s*{CONF_KEY}:", text):
            raise EsphomeError(
                f"{doc}:{line_no + 1} does not hold '{CONF_KEY}'; edit it by hand"
            )
        indent = text[: len(text) - len(text.lstrip())]
        return KeyEdit(doc, line_no, text, f"{indent}{rendered}", insert_after=True)
    # A bare block indents one level below the `encryption:` key
    doc, line_no = _editable_source(item, CONF_ENCRYPTION)
    lines = _read_text(doc).splitlines()
    block_text = lines[line_no] if line_no < len(lines) else ""
    if not re.fullmatch(rf"\s*{CONF_ENCRYPTION}:\s*(#.*)?", block_text):
        raise EsphomeError(
            f"{doc}:{line_no + 1} writes the block in flow style; edit it by hand"
        )
    indent = block_text[: len(block_text) - len(block_text.lstrip())] + "  "
    return KeyEdit(doc, line_no, block_text, f"{indent}{rendered}", insert_after=True)


def apply_key_edits(edits: list[KeyEdit]) -> dict[Path, str]:
    """Rewrite the located lines and return each touched file's original
    text for a rollback; a file that no longer loads is undone here."""
    from esphome import yaml_util
    from esphome.compiled_config import invalidate_compiled_config

    originals: dict[Path, str] = {}
    current: dict[Path, list[str]] = {}
    try:
        # Highest line first, so an insertion never shifts a later edit; an
        # insertion after a line goes before that line's own rewrite
        for edit in sorted(edits, key=lambda e: (e.line, e.insert_after), reverse=True):
            if edit.path not in originals:
                originals[edit.path] = _read_text(edit.path)
                current[edit.path] = originals[edit.path].splitlines(keepends=True)
            lines = current[edit.path]
            nl = "\r\n" if "\r\n" in originals[edit.path] else "\n"
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
                yaml_util.load_yaml(path)
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
