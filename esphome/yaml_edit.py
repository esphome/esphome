"""Rewrite the OTA encryption key where it lives in the user's yaml.

The key is located through the source ranges the yaml loader records on
every value, so the line to rewrite is the one the parser read the key
from: a `key:` line in the configuration, or the `secrets.yaml` line a
`!secret` on it points to. Only that line changes; quotes and comments on
it stay. The result is parsed again before it counts.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re

from esphome.components.noise import static_encryption_key
from esphome.const import CONF_API, CONF_ESPHOME, CONF_KEY, CONF_OTA, CONF_PLATFORM
from esphome.core import CORE, EsphomeError
from esphome.helpers import read_file, write_file
from esphome.types import ConfigType
from esphome.yaml_util import secrets_path_for


@dataclass
class KeyEdit:
    """One yaml line that holds the current key, and its replacement."""

    path: Path
    line: int
    new_line: str


def _key_nodes(raw: ConfigType, key: str) -> list[str]:
    """The api and esphome ota key values equal to ``key``; from a loaded
    yaml they carry the source range they were read from."""
    nodes = [static_encryption_key(raw.get(CONF_API) or {})]
    nodes += [
        static_encryption_key(item)
        for item in raw.get(CONF_OTA) or []
        if item.get(CONF_PLATFORM) == CONF_ESPHOME
    ]
    return [node for node in nodes if node is not None and str(node) == key]


def _key_line_re(prefix: str, key: str) -> re.Pattern[str]:
    """Match ``<prefix><quote><key><quote><comment>``, keeping every part
    around the key so the rewrite can put the new key in the same place."""
    return re.compile(rf"^({prefix})([\"']?){re.escape(key)}\2(\s*(?:#.*)?)$")


def _rewrite(match: re.Match[str], new_key: str) -> str:
    return f"{match.group(1)}{match.group(2)}{new_key}{match.group(2)}{match.group(3)}"


def _editable_source(node: str) -> tuple[Path, int]:
    """The file and line a key was read from, refusing sources that cannot
    be edited in place."""
    rng = getattr(node, "esp_range", None)
    if rng is None:
        raise EsphomeError(
            "The key has no source location; it comes from a package or "
            "substitution that cannot be edited in place"
        )
    doc = Path(rng.start_mark.document)
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
    return doc, rng.start_mark.line


def _secret_edit(doc: Path, name: str, old_key: str, new_key: str) -> KeyEdit:
    """The edit for a ``!secret name`` read from ``doc``: the one line in
    the secrets.yaml the loader would consult that defines the name with
    the current key."""
    secrets_path = secrets_path_for(doc)
    line_re = _key_line_re(rf"{re.escape(name)}:\s*", old_key)
    hits = [
        (i, m)
        for i, text in enumerate(read_file(secrets_path).splitlines())
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
    """Find every yaml line that carries the current key and prepare its
    rewrite. Raises EsphomeError for anything that cannot be edited line by
    line, such as a substitution, a flow mapping or a remote package."""
    literal_re = _key_line_re(rf"\s*{CONF_KEY}:\s*", old_key)
    secret_re = re.compile(rf"^\s*{CONF_KEY}:\s*!secret\s+([^\s#]+)")
    nodes = _key_nodes(CORE.raw_config or {}, old_key)
    if not nodes:
        raise EsphomeError("The current key was not found in the yaml")
    edits: list[KeyEdit] = []
    for node in nodes:
        doc, line_no = _editable_source(node)
        lines = read_file(doc).splitlines()
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


def apply_key_edits(edits: list[KeyEdit]) -> dict[Path, str]:
    """Rewrite the located lines, atomically per file, and return the
    original text of every touched file for a rollback.

    Every touched file is loaded again afterwards; a rewrite that broke the
    yaml is undone here, one that missed a node fails the compile that
    follows and is undone by the caller.
    """
    from esphome import yaml_util
    from esphome.compiled_config import invalidate_compiled_config

    originals: dict[Path, str] = {}
    current: dict[Path, list[str]] = {}
    try:
        for edit in edits:
            if edit.path not in originals:
                originals[edit.path] = read_file(edit.path)
                current[edit.path] = originals[edit.path].splitlines(keepends=True)
            lines = current[edit.path]
            ending = "\n" if lines[edit.line].endswith("\n") else ""
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
