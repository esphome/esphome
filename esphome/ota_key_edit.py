"""Rewrite the OTA encryption key in place: the `key:` line the loader read
it from, or the secrets.yaml line a `!secret` on it points to."""

from __future__ import annotations

from dataclasses import dataclass, field
import logging
import os
from pathlib import Path
import re

from esphome.components.noise import static_encryption_key
from esphome.const import CONF_API, CONF_ENCRYPTION, CONF_KEY
from esphome.core import CORE, EsphomeError
from esphome.espota2 import CONF_OLD_KEY, esphome_ota_items
from esphome.types import ConfigType
from esphome.yaml_edit import (
    LineEdit,
    editable_file,
    field_line_re,
    own_documents,
    read_text,
    rewrite,
    source_line,
    source_of,
)
from esphome.yaml_util import secrets_path_for

_LOGGER = logging.getLogger(__name__)
_DOMAIN = "ota_key_edit"


@dataclass
class KeyEdit(LineEdit):
    """A line edit with the secret it holds and the other configs using it."""

    secret: str | None = None
    shared_with: list[Path] = field(default_factory=list)


def _key_blocks(raw: ConfigType, key: str) -> list[ConfigType]:
    """The api and esphome ota ``encryption:`` mappings whose key is ``key``."""
    blocks = [raw.get(CONF_API) or {}, *esphome_ota_items(raw)]
    # A bare ota block gets the api key at validation; it has no key line
    return [
        block[CONF_ENCRYPTION]
        for block in blocks
        if (node := static_encryption_key(block)) is not None
        and str(node) == key
        and source_of(block[CONF_ENCRYPTION], CONF_KEY) is not None
    ]


_INDENT_RE = re.compile(r"\s*")
_KEY_FIELD_RE = re.compile(rf"^\s*{CONF_KEY}\s*:")
_BARE_BLOCK_RE = re.compile(rf"^\s*{CONF_ENCRYPTION}\s*:\s*(#.*)?$")
_YAML_SUFFIXES = (".yaml", ".yml")
_COMMENT_RE = re.compile(r"(?:^|\s)#.*$")
_INCLUDE_DIR_RE = re.compile(r"!include_dir_\w+\s+(?P<dir>[^\s#]+)")
# An include or package file whose path holds a substitution
_SUBSTITUTED_INCLUDE_RE = re.compile(
    r"(?:!include\w*\s+|\bfile:\s*)[\"']?(?P<target>[^\s#\"']*\$[^\s#\"']*)"
)


def _secret_ref(name: str = r"[^\s#\"']+") -> str:
    """`!secret name` as the whole scalar; `!secret ota key` is not `ota`."""
    return rf"!secret\s+(?P<q>[\"']?)(?P<name>{name})(?P=q)(?=\s*(?:#|$))"


def _secret_field_re(name: str) -> re.Pattern[str]:
    """``name: !secret <any>``."""
    return re.compile(rf"^\s*{name}\s*:\s*{_secret_ref()}")


def _secret_use_re(name: str) -> re.Pattern[str]:
    """A use of the secret ``name`` anywhere on a line."""
    return re.compile(_secret_ref(re.escape(name)))


_KEY_SECRET_RE = _secret_field_re(CONF_KEY)
_OLD_KEY_SECRET_RE = _secret_field_re(CONF_OLD_KEY)


def _indent(text: str) -> str:
    return _INDENT_RE.match(text).group()


def _without_comment(text: str) -> str:
    """The line up to its comment, which starts at a `#` after whitespace."""
    return _COMMENT_RE.sub("", text)


def _file_use_re(file_name: str) -> re.Pattern[str]:
    """The file name at a path boundary; `wifi_api.yaml` is not `api.yaml`."""
    return re.compile(rf"(?<![\w.-]){re.escape(file_name)}(?![\w.])")


def _root_indent(lines: list[str]) -> str:
    """The indent of the first content line; a root mapping may be indented."""
    content = (
        t for t in lines if t.strip() and not t.strip().startswith(("#", "---", "%"))
    )
    return next((_indent(t) for t in content), "")


def _config_texts() -> tuple[list[tuple[Path, str]], list[str]]:
    """Every yaml file under the config dir with its text, and what could
    not be read (a symlinked directory is not walked); walked once per run."""
    if (found := CORE.data.get(_DOMAIN)) is None:
        found = CORE.data[_DOMAIN] = _walk_config_texts()
    return found


def _walk_config_texts() -> tuple[list[tuple[Path, str]], list[str]]:
    data_dir = CORE.data_dir.resolve()
    found = []
    skipped: list[str] = []

    def skip(what: object) -> None:
        skipped.append(str(what))
        _LOGGER.debug("Skipping %s", what)

    for dirpath, dirnames, filenames in os.walk(
        CORE.config_dir.resolve(), onerror=skip
    ):
        for name in list(dirnames):
            path = Path(dirpath, name)
            if path == data_dir:
                dirnames.remove(name)
            elif path.is_symlink():
                dirnames.remove(name)
                skip(f"{path} is a link")
        for filename in filenames:
            path = Path(dirpath, filename)
            if path.suffix not in _YAML_SUFFIXES:
                continue
            try:
                text = read_text(path)
            except EsphomeError as err:
                skip(err)
                continue
            # Searched, never edited: comments do not count
            found.append((path, "\n".join(map(_without_comment, text.splitlines()))))
    return sorted(found), skipped


def with_sharers(edits: list[KeyEdit]) -> list[str]:
    """Fill in ``shared_with``: the other configs that use each rewritten
    line's secret or include, directly or through further includes. Returns
    what the scan could not follow (unreadable files, directory and
    substituted includes)."""
    main = CORE.config_path.resolve()
    own = own_documents()
    # This config's own includes stay in: another device may reach a line
    # through one of them
    texts, skipped = _config_texts()
    texts = [(path, text) for path, text in texts if path.resolve() != main]
    unchecked = [*skipped]
    for path, text in texts:
        if match := _INCLUDE_DIR_RE.search(text):
            unchecked.append(f"{path} includes the directory {match['dir']}")
        if match := _SUBSTITUTED_INCLUDE_RE.search(text):
            unchecked.append(
                f"{path} includes {match['target']} through a substitution"
            )
    for edit in edits:
        if edit.insert_after:
            continue
        ref = (
            _secret_use_re(edit.secret) if edit.secret else _file_use_re(edit.path.name)
        )
        users = {path for path, text in texts if ref.search(text)}
        frontier = users
        while frontier:
            frontier = {
                path
                for name in {p.name for p in frontier}
                for path, text in texts
                if path not in users and _file_use_re(name).search(text)
            }
            users |= frontier
        edit.shared_with = sorted(p for p in users if p.resolve() not in own)
    return unchecked


def _secret_file(doc: Path) -> Path:
    return editable_file(secrets_path_for(doc))


def _secret_line(
    secrets_path: Path, name: str, value: str | None = None
) -> tuple[int, re.Match[str]] | None:
    """The root level ``name:`` line, None when absent."""
    lines = read_text(secrets_path).splitlines()
    line_re = field_line_re(name, value, re.escape(_root_indent(lines)))
    return next(
        ((i, m) for i, text in enumerate(lines) if (m := line_re.match(text))), None
    )


def _secret_rewrite(
    secrets_path: Path, name: str, value: str, expect: str | None = None
) -> KeyEdit | None:
    """Set the root level ``name:`` line to ``value``; None when it is not a
    plain scalar holding ``expect``."""
    if (hit := _secret_line(secrets_path, name, expect)) is None:
        return None
    i, m = hit
    return KeyEdit(secrets_path, i, m.string, rewrite(m, value), secret=name)


def _secret_insert(secrets_path: Path, after: str, name: str, value: str) -> KeyEdit:
    """Add ``name: value`` under the root level ``after`` line."""
    if (hit := _secret_line(secrets_path, after)) is None:
        raise EsphomeError(
            f"No plain '{after}:' line in {secrets_path}; edit it by hand"
        )
    i, m = hit
    return KeyEdit(
        secrets_path,
        i,
        m.string,
        f'{_indent(m.string)}{name}: "{value}"',
        insert_after=True,
    )


def locate_key_edits(old_key: str, new_key: str) -> list[KeyEdit]:
    """The edits that replace the current key; refuses a substitution, flow
    mapping or remote package."""
    literal_re = field_line_re(CONF_KEY, old_key)
    blocks = _key_blocks(CORE.raw_config or {}, old_key)
    if not blocks:
        raise EsphomeError("The current key was not found on a line of the yaml")
    edits: list[KeyEdit] = []
    key_lines: set[tuple[Path, int]] = set()
    for block in blocks:
        doc, line_no, text = source_line(block, CONF_KEY)
        key_lines.add((doc.resolve(), line_no))
        if match := _KEY_SECRET_RE.match(text):
            name, secrets_path = match["name"], _secret_file(doc)
            edit = _secret_rewrite(secrets_path, name, new_key, expect=old_key)
            if edit is None:
                raise EsphomeError(
                    f"No '{name}:' line with the current key in {secrets_path}"
                )
        elif match := literal_re.match(text):
            edit = KeyEdit(doc, line_no, text, rewrite(match, new_key))
        else:
            raise EsphomeError(
                f"{doc}:{line_no + 1} does not hold the key as a plain value or "
                "a !secret; edit the key by hand"
            )
        # The api and ota blocks may share one !secret line
        if edit not in edits:
            edits.append(edit)
    for edit in edits:
        if edit.secret:
            _refuse_other_own_uses(edit.secret, key_lines)
    return edits


def _refuse_other_own_uses(name: str, allowed: set[tuple[Path, int]]) -> None:
    """The secret may only feed the ``allowed`` lines; a wifi password on it
    would change too."""
    ref = _secret_use_re(name)
    for doc in own_documents():
        for i, text in enumerate(read_text(doc).splitlines()):
            if ref.search(_without_comment(text)) and (doc, i) not in allowed:
                raise EsphomeError(
                    f"'{name}' is also used at {doc}:{i + 1}; a rotation would "
                    "change that value too, edit the key by hand"
                )


def old_key_edit(old_key: str, secret: str | None = None) -> list[KeyEdit]:
    """Set ``old_key:`` on the esphome ota block, rewriting or adding it. A
    key from secrets.yaml (``secret``) stays there as ``<secret>_old``. Of
    several entries the one with its own old_key or key line gets it."""
    items = [
        (item, item[CONF_ENCRYPTION] or {})
        for item in esphome_ota_items(CORE.raw_config or {})
        if CONF_ENCRYPTION in item
    ]
    if not items:
        raise EsphomeError("The esphome OTA platform has no 'encryption:' block")
    item, encryption = next(
        (
            pair
            for name in (CONF_OLD_KEY, CONF_KEY)
            for pair in items
            if source_of(pair[1], name) is not None
        ),
        items[0],
    )
    if (existing := encryption.get(CONF_OLD_KEY)) is not None:
        doc, line_no, text = source_line(encryption, CONF_OLD_KEY)
        if match := _OLD_KEY_SECRET_RE.match(text):
            name, secrets_path = match["name"], _secret_file(doc)
            if (edit := _secret_rewrite(secrets_path, name, old_key)) is None:
                raise EsphomeError(
                    f"No plain '{name}:' line in {secrets_path}; edit it by hand"
                )
            _refuse_other_own_uses(name, {(doc.resolve(), line_no)})
            return [edit]
        match = field_line_re(CONF_OLD_KEY, str(existing)).match(text)
        if match is None:
            raise EsphomeError(
                f"{doc}:{line_no + 1} does not hold '{CONF_OLD_KEY}' as a plain "
                "value; edit it by hand"
            )
        return [KeyEdit(doc, line_no, text, rewrite(match, old_key))]
    # Indent from the block's own key line, else one level under `encryption:`
    if source_of(encryption, CONF_KEY) is not None:
        mapping, anchor, anchor_re, step = encryption, CONF_KEY, _KEY_FIELD_RE, ""
    else:
        mapping, anchor, anchor_re, step = item, CONF_ENCRYPTION, _BARE_BLOCK_RE, "  "
    doc, line_no, text = source_line(mapping, anchor)
    if not anchor_re.match(text):
        raise EsphomeError(
            f"{doc}:{line_no + 1} does not hold '{anchor}:' as a block line; "
            "edit it by hand"
        )
    if secret is None:
        rendered = f'{CONF_OLD_KEY}: "{old_key}"'
        extra = []
    else:
        rendered = f"{CONF_OLD_KEY}: !secret {secret}_old"
        secrets_path = _secret_file(doc)  # the file that gets old_key
        # An existing `<secret>_old` line is reused only when nothing uses it
        if (
            kept := _secret_rewrite(secrets_path, f"{secret}_old", old_key)
        ) is not None:
            ref = _secret_use_re(f"{secret}_old")
            texts, skipped = _config_texts()
            if skipped:  # an unread file might use the line
                raise EsphomeError(
                    "Could not read every configuration to check the secret: "
                    + skipped[0]
                )
            if users := [p for p, t in texts if ref.search(t)]:
                raise EsphomeError(
                    f"'{secret}_old:' in {secrets_path} is used by "
                    + ", ".join(str(p) for p in users)
                    + f"; rename it or add '{CONF_OLD_KEY}' by hand"
                )
        extra = [kept or _secret_insert(secrets_path, secret, f"{secret}_old", old_key)]
    edit = KeyEdit(
        doc, line_no, text, f"{_indent(text)}{step}{rendered}", insert_after=True
    )
    return [edit, *extra]
