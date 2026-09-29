"""Rewrite the OTA encryption key in place: the `key:` line the loader read
it from, or the secrets.yaml line a `!secret` on it points to."""

from __future__ import annotations

from dataclasses import dataclass, field
import logging
import os
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


@dataclass
class KeyEdit(LineEdit):
    """A line edit that knows which secret it holds and who else uses it."""

    # The secrets file name this line holds, when it is one
    secret: str | None = None
    # Other configurations that use the rewritten secret or include, and
    # what the scan for them could not read
    shared_with: list[Path] = field(default_factory=list)
    unchecked: list[str] = field(default_factory=list)


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
        and source_of(block[CONF_ENCRYPTION], CONF_KEY) is not None
    ]


_INDENT_RE = re.compile(r"\s*")
# `!secret name`, the name optionally quoted
# The whole scalar, so `!secret ota key` is not read as the secret `ota`
_SECRET_REF = r"!secret\s+(?P<q>[\"']?)(?P<name>[^\s#\"']+)(?P=q)(?=\s*(?:#|$))"
_KEY_SECRET_RE = re.compile(rf"^\s*{CONF_KEY}\s*:\s*{_SECRET_REF}")
_OLD_KEY_SECRET_RE = re.compile(rf"^\s*{CONF_OLD_KEY}\s*:\s*{_SECRET_REF}")
_KEY_FIELD_RE = re.compile(rf"^\s*{CONF_KEY}\s*:")
_BARE_BLOCK_RE = re.compile(rf"^\s*{CONF_ENCRYPTION}\s*:\s*(#.*)?$")
_YAML_SUFFIXES = (".yaml", ".yml")
_COMMENT_RE = re.compile(r"(?:^|\s)#.*$")
_INCLUDE_DIR_RE = re.compile(r"!include_dir_\w+\s+(?P<dir>[^\s#]+)")
# An include or package file whose path holds a substitution
_SUBSTITUTED_INCLUDE_RE = re.compile(
    r"(?:!include\w*\s+|\bfile:\s*)[\"']?(?P<target>[^\s#\"']*\$[^\s#\"']*)"
)


def _indent(text: str) -> str:
    return _INDENT_RE.match(text).group()


def _without_comment(text: str) -> str:
    """The line up to its comment, which starts at a `#` after whitespace."""
    return _COMMENT_RE.sub("", text)


def _secret_use_re(name: str) -> re.Pattern[str]:
    return re.compile(rf"!secret\s+[\"']?{re.escape(name)}[\"']?(?=[\s#]|$)")


def _file_use_re(file_name: str) -> re.Pattern[str]:
    """The file name at a path boundary; `wifi_api.yaml` is not `api.yaml`."""
    return re.compile(rf"(?<![\w.-]){re.escape(file_name)}(?![\w.])")


def _root_indent(lines: list[str]) -> str:
    """The indent of the first content line: a secrets file may indent its
    whole root mapping."""
    content = (
        t for t in lines if t.strip() and not t.strip().startswith(("#", "---", "%"))
    )
    return next((_indent(t) for t in content), "")


def _other_config_texts(
    own: set[Path], strict: bool = False
) -> tuple[list[tuple[Path, str]], list[str]]:
    """Every yaml file in the configuration directory outside ``own`` and
    the build data, with its text, and what could not be read. A symlinked
    directory is not walked and counts as unread. With ``strict`` anything
    unread is refused, for a check that must not fail open."""
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
            if path.suffix not in _YAML_SUFFIXES or path.resolve() in own:
                continue
            try:
                text = read_text(path)
            except EsphomeError as err:
                skip(err)
                continue
            # Searched, never edited: comments do not count
            found.append((path, "\n".join(map(_without_comment, text.splitlines()))))
    if strict and skipped:
        raise EsphomeError(
            "Could not read every configuration to check the secret: " + skipped[0]
        )
    return sorted(found), skipped


def _with_sharers(edits: list[KeyEdit]) -> list[KeyEdit]:
    """Fill in the other configurations each rewritten line serves: users of
    its secret, or of the include it sits in, and the files that include
    those in turn; an added line serves none. The search starts from every
    file but this configuration's own, this configuration's includes among
    them, since another device may reach the line through one of those. A
    directory include cannot be followed by name and is reported as unread."""
    main = CORE.config_path.resolve()
    # The main file counts too: another device may include it
    refs = {
        id(edit): _secret_use_re(edit.secret)
        if edit.secret
        else _file_use_re(edit.path.name)
        for edit in edits
        if not edit.insert_after
    }
    if not refs:
        return edits
    own = own_documents()
    texts, skipped = _other_config_texts({main})
    for path, text in texts:
        if match := _INCLUDE_DIR_RE.search(text):
            skipped.append(f"{path} includes the directory {match['dir']}")
        if match := _SUBSTITUTED_INCLUDE_RE.search(text):
            skipped.append(f"{path} includes {match['target']} through a substitution")
    for edit in edits:
        if (ref := refs.get(id(edit))) is None:
            continue
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
        edit.unchecked = skipped
    return edits


def _secret_file(doc: Path) -> Path:
    return editable_file(secrets_path_for(doc))


def _secret_line(
    secrets_path: Path, name: str, value: str | None = None
) -> tuple[int, re.Match[str]] | None:
    """The root level ``name:`` line, None when absent; the loader already
    refused a duplicate key."""
    lines = read_text(secrets_path).splitlines()
    line_re = field_line_re(name, value, re.escape(_root_indent(lines)))
    return next(
        ((i, m) for i, text in enumerate(lines) if (m := line_re.match(text))), None
    )


def _secret_rewrite(
    secrets_path: Path, name: str, value: str, expect: str | None = None
) -> KeyEdit | None:
    """Set the root level ``name:`` line to ``value``; None when the line
    is not a plain scalar, or with ``expect``, does not hold it."""
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
    """The edits that replace the current key; raises EsphomeError for a
    substitution, flow mapping or remote package."""
    literal_re = field_line_re(CONF_KEY, old_key)
    blocks = _key_blocks(CORE.raw_config or {}, old_key)
    if not blocks:
        raise EsphomeError("The current key was not found on a line of the yaml")
    edits: list[KeyEdit] = []
    for block in blocks:
        doc, line_no, text = source_line(block, CONF_KEY)
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
    key_lines = {
        (doc.resolve(), line)
        for doc, line, _ in (source_line(b, CONF_KEY) for b in blocks)
    }
    for edit in edits:
        if edit.secret:
            _refuse_other_own_uses(edit.secret, key_lines)
    return _with_sharers(edits)


def _refuse_other_own_uses(name: str, allowed: set[tuple[Path, int]]) -> None:
    """The secret may only feed the ``allowed`` lines: a wifi password on
    the same secret would change with it and take the device off the
    network."""
    ref = _secret_use_re(name)
    for doc in own_documents():
        for i, text in enumerate(read_text(doc).splitlines()):
            if ref.search(_without_comment(text)) and (doc, i) not in allowed:
                raise EsphomeError(
                    f"'{name}' is also used at {doc}:{i + 1}; a rotation would "
                    "change that value too, edit the key by hand"
                )


def _key_secret(old_key: str) -> tuple[Path, str] | None:
    """The file and name of a ``key: !secret name`` line holding the key."""
    for block in _key_blocks(CORE.raw_config or {}, old_key):
        doc, _, text = source_line(block, CONF_KEY)
        if match := _KEY_SECRET_RE.match(text):
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
    # An entry that already carries old_key wins over one that carries key
    item = next(
        (
            item
            for name in (CONF_OLD_KEY, CONF_KEY)
            for item in items
            if source_of(item[CONF_ENCRYPTION] or {}, name) is not None
        ),
        items[0],
    )
    encryption = item[CONF_ENCRYPTION] or {}
    if (existing := encryption.get(CONF_OLD_KEY)) is not None:
        doc, line_no, text = source_line(encryption, CONF_OLD_KEY)
        if match := _OLD_KEY_SECRET_RE.match(text):
            name, secrets_path = match["name"], _secret_file(doc)
            if (edit := _secret_rewrite(secrets_path, name, old_key)) is None:
                raise EsphomeError(
                    f"No plain '{name}:' line in {secrets_path}; edit it by hand"
                )
            _refuse_other_own_uses(name, {(doc.resolve(), line_no)})
            return _with_sharers([edit])
        match = field_line_re(CONF_OLD_KEY, str(existing)).match(text)
        if match is None:
            raise EsphomeError(
                f"{doc}:{line_no + 1} does not hold '{CONF_OLD_KEY}' as a plain "
                "value; edit it by hand"
            )
        return _with_sharers([KeyEdit(doc, line_no, text, rewrite(match, old_key))])
    # The anchor sets the indent: the block's own key line, in whichever
    # file holds it, else one level under a bare `encryption:` line
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
    if (secret := _key_secret(old_key)) is None:
        rendered = f'{CONF_OLD_KEY}: "{old_key}"'
        extra = []
    else:
        name = secret[1]
        rendered = f"{CONF_OLD_KEY}: !secret {name}_old"
        # `!secret <name>_old` resolves from the file that gets old_key
        secrets_path = _secret_file(doc)
        # This configuration has no old_key, so any use of an existing
        # `<name>_old` line, its own included, is another setting; a line
        # nobody uses is a leftover of an earlier rotation
        if (kept := _secret_rewrite(secrets_path, f"{name}_old", old_key)) is not None:
            ref = _secret_use_re(f"{name}_old")
            texts, _ = _other_config_texts(set(), strict=True)
            if users := [p for p, t in texts if ref.search(t)]:
                raise EsphomeError(
                    f"'{name}_old:' in {secrets_path} is used by "
                    + ", ".join(str(p) for p in users)
                    + f"; rename it or add '{CONF_OLD_KEY}' by hand"
                )
        extra = [kept or _secret_insert(secrets_path, name, f"{name}_old", old_key)]
    edit = KeyEdit(
        doc, line_no, text, f"{_indent(text)}{step}{rendered}", insert_after=True
    )
    return _with_sharers([edit, *extra])
