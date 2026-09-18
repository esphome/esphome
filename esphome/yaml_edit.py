"""Rewrite the OTA encryption key in place: the `key:` line the loader read
it from, or the secrets.yaml line a `!secret` on it points to."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re

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
    """One yaml line to rewrite, or a line to add right after it."""

    path: Path
    line: int
    new_line: str
    insert_after: bool = False


def _esphome_ota_item(raw: ConfigType) -> ConfigType | None:
    """The esphome ota item; a raw ``ota:`` may be a mapping, not a list."""
    ota = raw.get(CONF_OTA) or []
    items = [ota] if isinstance(ota, dict) else ota
    return next(
        (
            item
            for item in items
            if isinstance(item, dict) and item.get(CONF_PLATFORM) == CONF_ESPHOME
        ),
        None,
    )


def _key_blocks(raw: ConfigType, key: str) -> list[ConfigType]:
    """The api and esphome ota ``encryption:`` mappings whose key is ``key``."""
    blocks = [raw.get(CONF_API) or {}]
    if (item := _esphome_ota_item(raw)) is not None:
        blocks.append(item)
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


def _editable_source(mapping: ConfigType, name: str) -> tuple[Path, int]:
    """The file and line ``name:`` was read from; refuses uneditable sources."""
    if (source := _source_of(mapping, name)) is None:
        raise EsphomeError(f"'{name}' was not read from a file")
    doc, line_no = source
    resolved = doc.resolve()
    if (
        not doc.is_file()
        or not resolved.is_relative_to(CORE.config_dir.resolve())
        or resolved.is_relative_to(CORE.data_dir.resolve())
    ):
        raise EsphomeError(
            f"The key comes from {doc}, which is not an editable file in the "
            "configuration directory"
        )
    return doc, line_no


def _secret_edit(doc: Path, name: str, old_key: str, new_key: str) -> KeyEdit:
    """The secrets.yaml line ``!secret name`` in ``doc`` resolves to."""
    secrets_path = secrets_path_for(doc)
    line_re = _key_line_re(rf"{re.escape(name)}:\s*", old_key)
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
    return KeyEdit(secrets_path, i, _rewrite(m, new_key))


def locate_key_edits(old_key: str, new_key: str) -> list[KeyEdit]:
    """The edits that replace the current key; raises EsphomeError for a
    substitution, flow mapping or remote package."""
    literal_re = _key_line_re(rf"\s*{CONF_KEY}:\s*", old_key)
    secret_re = re.compile(rf"^\s*{CONF_KEY}:\s*!secret\s+([^\s#]+)")
    blocks = _key_blocks(CORE.raw_config or {}, old_key)
    if not blocks:
        raise EsphomeError("The current key was not found on a line of the yaml")
    edits: list[KeyEdit] = []
    for block in blocks:
        doc, line_no = _editable_source(block, CONF_KEY)
        lines = _read_text(doc).splitlines()
        text = lines[line_no] if line_no < len(lines) else ""
        if match := secret_re.match(text):
            edit = _secret_edit(doc, match.group(1), old_key, new_key)
        elif match := literal_re.match(text):
            edit = KeyEdit(doc, line_no, _rewrite(match, new_key))
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
    """Set ``old_key:`` on the esphome ota block, rewriting or adding it."""
    item = _esphome_ota_item(CORE.raw_config or {})
    if item is None or CONF_ENCRYPTION not in item:
        raise EsphomeError("The esphome OTA platform has no 'encryption:' block")
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
        return KeyEdit(doc, line_no, _rewrite(match, old_key))
    # The block's own key line sets the indent; a bare block indents one
    # level below the `encryption:` key
    doc, line_no = _editable_source(item, CONF_ENCRYPTION)
    lines = _read_text(doc).splitlines()
    if (key_source := _source_of(encryption, CONF_KEY)) is not None:
        _, key_line = key_source
        key_text = lines[key_line]
        indent = key_text[: len(key_text) - len(key_text.lstrip())]
        return KeyEdit(doc, key_line, f"{indent}{rendered}", insert_after=True)
    block_text = lines[line_no]
    indent = block_text[: len(block_text) - len(block_text.lstrip())] + "  "
    return KeyEdit(doc, line_no, f"{indent}{rendered}", insert_after=True)


def apply_key_edits(edits: list[KeyEdit]) -> dict[Path, str]:
    """Rewrite the located lines and return each touched file's original
    text for a rollback; a file that no longer loads is undone here."""
    from esphome import yaml_util
    from esphome.compiled_config import invalidate_compiled_config

    originals: dict[Path, str] = {}
    current: dict[Path, list[str]] = {}
    try:
        # Highest line first, so an insertion never shifts a later edit
        for edit in sorted(edits, key=lambda e: e.line, reverse=True):
            if edit.path not in originals:
                originals[edit.path] = _read_text(edit.path)
                current[edit.path] = originals[edit.path].splitlines(keepends=True)
            lines = current[edit.path]
            nl = "\r\n" if "\r\n" in originals[edit.path] else "\n"
            text = lines[edit.line].rstrip("\r\n")
            ending = lines[edit.line][len(text) :]
            if edit.insert_after:
                lines[edit.line] = text + nl
                lines.insert(edit.line + 1, edit.new_line + ending)
            else:
                lines[edit.line] = edit.new_line + ending
        for path, lines in current.items():
            write_file(path, "".join(lines))
        for path in originals:
            try:
                yaml_util.load_yaml(path)
            except EsphomeError as err:
                raise EsphomeError(f"{path} no longer loads: {err}") from err
    except EsphomeError:
        for path, text in originals.items():
            write_file(path, text)
        raise
    invalidate_compiled_config()
    return originals


def restore_key_files(originals: dict[Path, str]) -> None:
    """Put back the files apply_key_edits rewrote."""
    from esphome.compiled_config import invalidate_compiled_config

    for path, text in originals.items():
        write_file(path, text)
    invalidate_compiled_config()
