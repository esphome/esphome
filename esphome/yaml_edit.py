"""Rewrite the OTA encryption key in place: the `key:` line the loader read
it from, or the secrets.yaml line a `!secret` on it points to."""

from __future__ import annotations

from dataclasses import dataclass, field
import logging
import os
from pathlib import Path
import re
import stat

from esphome import compiled_config, yaml_util
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
    # The secrets file name this line holds, when it is one
    secret: str | None = None
    # Other configurations that use the rewritten secret or include, and
    # what the scan for them could not read
    shared_with: list[Path] = field(default_factory=list)
    unchecked: list[str] = field(default_factory=list)


@dataclass
class Snapshot:
    """A rewritten file's text before and after; the restore only puts
    ``original`` back over ``written``, never over the user's own edit."""

    original: str
    written: str


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
    if line_no >= len(lines):
        raise EsphomeError(f"{doc}:{line_no + 1} changed since it was read")
    return lines[line_no]


def _own_documents() -> set[Path]:
    """Every file the loader read for this configuration."""
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
    return {Path(document).resolve() for document in documents}


_INDENT_RE = re.compile(r"\s*")
# A plain scalar with no yaml indicator, so a block scalar (`>-`, `|`) or a
# flow collection never counts; then optional matching quotes and the trailer
# An empty value never counts either, and a comment needs whitespace before
# its `#`, as the loader reads it
_PLAIN_SCALAR = r"[^\s#\"'>|&*!%@`\[\]{},]+"
_TRAILER = r"(?P<quote>[\"']?){value}(?P=quote)(?P<trail>(?:\s+#.*)?\s*)$"
# `!secret name`, the name optionally quoted
_SECRET_REF = r"!secret\s+(?P<q>[\"']?)(?P<name>[^\s#\"']+)(?P=q)"
_KEY_SECRET_RE = re.compile(rf"^\s*{CONF_KEY}\s*:\s*{_SECRET_REF}")
_OLD_KEY_SECRET_RE = re.compile(rf"^\s*{CONF_OLD_KEY}\s*:\s*{_SECRET_REF}")
_KEY_FIELD_RE = re.compile(rf"^\s*{CONF_KEY}\s*:")
_BARE_BLOCK_RE = re.compile(rf"^\s*{CONF_ENCRYPTION}\s*:\s*(#.*)?$")
_YAML_SUFFIXES = (".yaml", ".yml")


def _indent(text: str) -> str:
    return _INDENT_RE.match(text).group()


def _field_line_re(
    name: str, value: str | None = None, indent: str = r"\s*"
) -> re.Pattern[str]:
    """Match ``name: value``, or ``name:`` with any plain scalar, at
    ``indent``; the name may be quoted. Keeps the prefix, the quotes and the
    trailer for _rewrite."""
    scalar = _PLAIN_SCALAR if value is None else re.escape(value)
    prefix = rf"{indent}[\"']?{re.escape(name)}[\"']?\s*:\s*"
    return re.compile(rf"^(?P<prefix>{prefix}){_TRAILER.format(value=scalar)}")


def _secret_use_re(name: str) -> re.Pattern[str]:
    return re.compile(rf"!secret\s+[\"']?{re.escape(name)}[\"']?(?=[\s#]|$)")


def _file_use_re(file_name: str) -> re.Pattern[str]:
    """The file name at a path boundary; `wifi_api.yaml` is not `api.yaml`."""
    return re.compile(rf"(?<![\w.-]){re.escape(file_name)}(?![\w.])")


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


def _source_line(mapping: ConfigType, name: str) -> tuple[Path, int, str]:
    """The file, line number and text ``name:`` was read from; refuses a
    source that was not read from an editable file."""
    if (source := _source_of(mapping, name)) is None:
        raise EsphomeError(f"'{name}' was not read from a file")
    doc, line_no = source
    # Checked resolved, returned as the loader saw it: a `!secret` in a
    # symlinked include resolves beside the link, not beside its target
    _editable_file(doc)
    return doc, line_no, _line_at(doc, line_no)


def _write_keeping_mode(path: Path, text: str) -> None:
    """secrets.yaml is often 0600; write_file would widen it to 0644."""
    try:
        mode = stat.S_IMODE(path.stat().st_mode)
        write_file(path, text, private=True)
        path.chmod(mode)
    except OSError as err:
        raise EsphomeError(f"Could not keep the mode of {path}: {err}") from err


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
                found.append((path, _read_text(path)))
            except EsphomeError as err:
                skip(err)
    if strict and skipped:
        raise EsphomeError(
            "Could not read every configuration to check the secret: " + skipped[0]
        )
    return sorted(found), skipped


def _with_sharers(edits: list[KeyEdit]) -> list[KeyEdit]:
    """Fill in the other configurations each rewritten line serves: users of
    its secret, or of the include it sits in, and the files that include
    those in turn; an added line serves none."""
    main = CORE.config_path.resolve()
    refs = {
        id(edit): _secret_use_re(edit.secret)
        if edit.secret
        else _file_use_re(edit.path.name)
        for edit in edits
        if not edit.insert_after and (edit.secret or edit.path.resolve() != main)
    }
    if not refs:
        return edits
    texts, skipped = _other_config_texts(_own_documents())
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
        edit.shared_with = sorted(users)
        edit.unchecked = skipped
    return edits


def _secret_file(doc: Path) -> Path:
    return _editable_file(secrets_path_for(doc))


def _secret_line(
    secrets_path: Path, name: str, value: str | None = None
) -> tuple[int, re.Match[str]] | None:
    """The root level ``name:`` line, None when absent; the loader already
    refused a duplicate key."""
    lines = _read_text(secrets_path).splitlines()
    line_re = _field_line_re(name, value, re.escape(_root_indent(lines)))
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
    return KeyEdit(secrets_path, i, m.string, _rewrite(m, value), secret=name)


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
    literal_re = _field_line_re(CONF_KEY, old_key)
    blocks = _key_blocks(CORE.raw_config or {}, old_key)
    if not blocks:
        raise EsphomeError("The current key was not found on a line of the yaml")
    edits: list[KeyEdit] = []
    for block in blocks:
        doc, line_no, text = _source_line(block, CONF_KEY)
        if match := _KEY_SECRET_RE.match(text):
            name, secrets_path = match["name"], _secret_file(doc)
            edit = _secret_rewrite(secrets_path, name, new_key, expect=old_key)
            if edit is None:
                raise EsphomeError(
                    f"No '{name}:' line with the current key in {secrets_path}"
                )
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
    for edit in edits:
        if edit.secret:
            _refuse_other_own_uses(edit.secret, blocks)
    return _with_sharers(edits)


def _refuse_other_own_uses(name: str, blocks: list[ConfigType]) -> None:
    """The secret may only feed the key lines: a wifi password on the same
    secret would change with it and take the device off the network."""
    key_lines = {
        (doc.resolve(), line)
        for doc, line, _ in (_source_line(b, CONF_KEY) for b in blocks)
    }
    ref = _secret_use_re(name)
    for doc in _own_documents():
        for i, text in enumerate(_read_text(doc).splitlines()):
            if ref.search(text) and (doc, i) not in key_lines:
                raise EsphomeError(
                    f"'{name}' is also used at {doc}:{i + 1}; a rotation would "
                    "change that value too, edit the key by hand"
                )


def _key_secret(old_key: str) -> tuple[Path, str] | None:
    """The file and name of a ``key: !secret name`` line holding the key."""
    for block in _key_blocks(CORE.raw_config or {}, old_key):
        doc, _, text = _source_line(block, CONF_KEY)
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
            if _source_of(item[CONF_ENCRYPTION] or {}, name) is not None
        ),
        items[0],
    )
    encryption = item[CONF_ENCRYPTION] or {}
    if (existing := encryption.get(CONF_OLD_KEY)) is not None:
        doc, line_no, text = _source_line(encryption, CONF_OLD_KEY)
        if match := _OLD_KEY_SECRET_RE.match(text):
            name, secrets_path = match["name"], _secret_file(doc)
            if (edit := _secret_rewrite(secrets_path, name, old_key)) is None:
                raise EsphomeError(
                    f"No plain '{name}:' line in {secrets_path}; edit it by hand"
                )
            return _with_sharers([edit])
        match = _field_line_re(CONF_OLD_KEY, str(existing)).match(text)
        if match is None:
            raise EsphomeError(
                f"{doc}:{line_no + 1} does not hold '{CONF_OLD_KEY}' as a plain "
                "value; edit it by hand"
            )
        return _with_sharers([KeyEdit(doc, line_no, text, _rewrite(match, old_key))])
    # The anchor sets the indent: the block's own key line, in whichever
    # file holds it, else one level under a bare `encryption:` line
    if _source_of(encryption, CONF_KEY) is not None:
        mapping, anchor, anchor_re, step = encryption, CONF_KEY, _KEY_FIELD_RE, ""
    else:
        mapping, anchor, anchor_re, step = item, CONF_ENCRYPTION, _BARE_BLOCK_RE, "  "
    doc, line_no, text = _source_line(mapping, anchor)
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


def _rewritten_text(original: str, edits: list[KeyEdit]) -> str:
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


def apply_key_edits(edits: list[KeyEdit]) -> dict[Path, Snapshot]:
    """Rewrite the located lines in place and return each touched file's
    text before and after, for a rollback. Every file is rewritten in
    memory before any is written, so a stale line touches nothing; a file
    that no longer loads is undone here."""
    by_path: dict[Path, list[KeyEdit]] = {}
    for edit in edits:
        # Resolved here as well, so a hand-built edit cannot reach a symlink
        # itself or a file outside the configuration directory
        by_path.setdefault(_editable_file(edit.path), []).append(edit)
    originals = {}
    for path, own in by_path.items():
        original = _read_text(path)
        originals[path] = Snapshot(original, _rewritten_text(original, own))
    try:
        for path, snapshot in originals.items():
            _write_keeping_mode(path, snapshot.written)
        for path in originals:
            try:
                yaml_util.load_yaml(path, track_document_range=False)
            except EsphomeError as err:
                raise EsphomeError(f"{path} no longer loads: {err}") from err
        compiled_config.invalidate_compiled_config()
    except BaseException as err:
        try:
            restore_key_files(originals)
        except EsphomeError as restore_err:
            if not isinstance(err, Exception):
                err.add_note(str(restore_err))  # an interrupt stays one
                raise err from None
            raise EsphomeError(f"{err}; {restore_err}") from err
        raise
    return originals


def restore_key_files(originals: dict[Path, Snapshot]) -> None:
    """Put back the files apply_key_edits rewrote; every file is tried and
    the ones that failed, or that the user changed meanwhile, are reported
    together and left alone."""
    failed = []
    for path, snapshot in originals.items():
        try:
            current = _read_text(path)
            if current == snapshot.original:
                continue  # never written, or already put back
            if current != snapshot.written:
                raise EsphomeError("changed since it was written, left as is")
            _write_keeping_mode(path, snapshot.original)
        except EsphomeError as err:
            failed.append(f"{path}: {err}")
    try:
        compiled_config.invalidate_compiled_config()
    except EsphomeError as err:
        failed.append(str(err))
    if failed:
        raise EsphomeError("Could not restore " + "; ".join(failed))
