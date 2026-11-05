"""Rewrite the OTA encryption key in place: the `key:` line the loader read
it from, or the secrets.yaml line a `!secret` on it points to."""

from __future__ import annotations

from collections.abc import Collection
from dataclasses import dataclass, field
import logging
import os
from pathlib import Path
import re

import yaml

from esphome.components.noise import static_encryption_key
from esphome.const import CONF_API, CONF_ENCRYPTION, CONF_KEY
from esphome.core import CORE, EsphomeError
from esphome.espota2 import CONF_OLD_KEY, esphome_ota_items
from esphome.types import ConfigType
from esphome.util import filter_yaml_files
from esphome.yaml_edit import (
    LineEdit,
    editable_file,
    field_line_re,
    field_prefix,
    own_documents,
    read_text,
    rewrite,
    secret_field_re,
    source_line,
    source_of,
)
from esphome.yaml_util import SECRET_YAML, secrets_path_for

_LOGGER = logging.getLogger(__name__)
_DOMAIN = "ota_key_edit"


@dataclass
class KeyEdit(LineEdit):
    """A line edit with the secret it holds and the other configs that may use it."""

    secret: str | None = None
    shared_with: list[Path] = field(default_factory=list)


def _key_blocks(raw: ConfigType, key: str) -> list[ConfigType]:
    """The api and esphome ota ``encryption:`` mappings whose key is ``key``."""
    blocks = [raw.get(CONF_API) or {}, *esphome_ota_items(raw)]
    found = []
    for block in blocks:
        if (node := static_encryption_key(block)) is None or str(node) != key:
            continue
        encryption = block[CONF_ENCRYPTION]
        if source_of(encryption, CONF_KEY) is not None:
            found.append(encryption)
        elif hasattr(next(k for k in encryption if k == CONF_KEY), "esp_range"):
            # Read but not on a line of its own, as with a merge key; a key
            # validation added has no range and is passed over
            raise EsphomeError(
                f"The '{CONF_KEY}' of an '{CONF_ENCRYPTION}:' block is not on a "
                "line of its own; edit the key by hand"
            )
    return found


_INDENT_RE = re.compile(r"\s*")
_KEY_FIELD_RE = re.compile(rf"^{field_prefix(CONF_KEY)}")
_BARE_BLOCK_RE = re.compile(rf"^{field_prefix(CONF_ENCRYPTION)}\s*(#.*)?$")
_COLLECTION_STARTS = (
    yaml.BlockMappingStartToken,
    yaml.BlockSequenceStartToken,
    yaml.FlowMappingStartToken,
    yaml.FlowSequenceStartToken,
)
_COLLECTION_ENDS = (
    yaml.BlockEndToken,
    yaml.FlowMappingEndToken,
    yaml.FlowSequenceEndToken,
)
_KEY_SECRET_RE = secret_field_re(CONF_KEY)


def _indent(text: str) -> str:
    return _INDENT_RE.match(text).group()


@dataclass
class _Scan:
    """What a yaml file names, from its tokens alone; comments never count."""

    secrets: list[tuple[str, int]] = field(default_factory=list)  # name, line
    targets: set[Path] = field(default_factory=set)  # local yaml files it names
    include_dir: str | None = None
    substituted: str | None = None  # an include path holding a substitution

    def uses(self, name: str) -> bool:
        return any(secret == name for secret, _ in self.secrets)


@dataclass
class _Configs:
    files: list[tuple[Path, _Scan]]
    skipped: list[str]
    named_by: dict[Path, set[Path]]  # a resolved file to the files naming it


@dataclass
class _State:
    scans: dict[Path, _Scan] = field(default_factory=dict)
    configs: _Configs | None = None
    secrets_paths: dict[Path, Path] = field(default_factory=dict)  # by directory


def _state() -> _State:
    return CORE.data.setdefault(_DOMAIN, _State())


def _scan(path: Path) -> _Scan:
    """``path`` scanned once per run; nothing is constructed, included or loaded."""
    scans = _state().scans
    if (found := scans.get(path)) is not None:
        return found
    if "!" not in (text := read_text(path)):  # names come only from tags
        found = scans[path] = _Scan()
        return found
    try:
        tokens = list(yaml.scan(text, Loader=yaml.SafeLoader))
    except yaml.YAMLError as err:
        raise EsphomeError(
            f"Could not scan {path}: {getattr(err, 'problem', err)}"
        ) from err
    found = scans[path] = _Scan()
    depth = 0
    include_maps: set[int] = set()  # depths of `!include {file: ...}` mappings
    keys: dict[int, str] = {}  # the last key seen at each depth
    for prev2, prev, token in zip(
        [None, None, *tokens], [None, *tokens], tokens, strict=False
    ):
        tag = prev.value[1] if isinstance(prev, yaml.TagToken) else ""
        if isinstance(token, _COLLECTION_STARTS):
            depth += 1
            if tag == "include":
                include_maps.add(depth)
            continue
        if isinstance(token, _COLLECTION_ENDS):
            include_maps.discard(depth)
            keys.pop(depth, None)
            depth -= 1
            continue
        if not isinstance(token, yaml.ScalarToken):
            continue
        value = token.value
        if isinstance(prev, yaml.KeyToken):
            keys[depth] = value
            continue
        # Only an include loads a local file; a remote package's file is not one
        included = tag == "include" or (
            depth in include_maps
            and isinstance(prev, yaml.ValueToken)
            and isinstance(prev2, yaml.ScalarToken)
            and keys.get(depth) == "file"
        )
        if tag == "secret":
            found.secrets.append((value, prev.start_mark.line))
        elif tag.startswith("include_dir_") and found.include_dir is None:
            found.include_dir = value
        elif included and "$" in value:
            found.substituted = found.substituted or value
        elif included:
            found.targets.add((path.parent / value).resolve())
    return found


def _child_indent(doc: Path, line_no: int, indent: str) -> str:
    """The indent of the first option under the block line, or one level more."""
    for text in read_text(doc).splitlines()[line_no + 1 :]:
        if not (stripped := text.strip()) or stripped.startswith("#"):
            continue
        if len(child := _indent(text)) > len(indent):
            return child
        break
    return f"{indent}  "


def _root_indent(lines: list[str]) -> str:
    """The indent of the first content line; a root mapping may be indented."""
    content = (
        t for t in lines if t.strip() and not t.strip().startswith(("#", "---", "%"))
    )
    return next((_indent(t) for t in content), "")


def _configs() -> _Configs:
    """Every yaml file under the config dir scanned, and what could not be;
    walked once per run."""
    state = _state()
    if state.configs is not None:
        return state.configs
    data_dir = CORE.data_dir.resolve()
    found: list[tuple[Path, _Scan]] = []
    skipped: list[str] = []

    def skip(what: object) -> None:
        skipped.append(str(what))
        _LOGGER.debug("Skipping %s", what)

    for dirpath, dirnames, filenames in os.walk(
        CORE.config_dir.resolve(), onerror=skip
    ):
        for name in list(dirnames):
            path = Path(dirpath, name)
            # Hidden trees (.git, .venv) hold no configurations, as in find_files
            if name.startswith(".") or path.resolve() == data_dir:
                dirnames.remove(name)
            elif path.is_symlink():
                dirnames.remove(name)
                skip(f"{path} is a link")
        for path in filter_yaml_files(Path(dirpath, f) for f in filenames):
            try:
                found.append((path, _scan(path)))
            except EsphomeError as err:
                skip(err)
    found.sort(key=lambda ps: ps[0])
    named_by: dict[Path, set[Path]] = {}
    for path, scan in found:
        for target in scan.targets:
            named_by.setdefault(target, set()).add(path)
    state.configs = _Configs(found, skipped, named_by)
    return state.configs


def _secrets_path(doc: Path) -> Path:
    """``secrets_path_for`` once per directory; it loads the secrets file."""
    paths = _state().secrets_paths
    if (found := paths.get(doc.parent)) is None:
        found = paths[doc.parent] = secrets_path_for(doc)
    return found


def _secret_users(name: str, secrets_path: Path) -> set[Path]:
    """The scanned files that use the secret ``name`` of ``secrets_path``,
    counting those that fall back to the main secrets file."""
    target = secrets_path.resolve()
    return {
        path
        for path, scan in _configs().files
        if scan.uses(name)
        and (
            (own := _secrets_path(path)) != path.parent / SECRET_YAML
            or own.resolve() == target
        )
    }


def with_sharers(edits: list[KeyEdit]) -> list[str]:
    """Fill in ``shared_with``: the other configs that use or may use each
    rewritten line's secret or include, directly or through further includes.
    Returns what the scan could not follow."""
    main = CORE.config_path.resolve()
    own = own_documents()
    # This config's own files stay in the search: another device may reach a
    # line through one of them, the main file included
    configs = _configs()
    unchecked = [*configs.skipped]
    for path, scan in configs.files:
        if path.resolve() == main:
            continue
        if scan.include_dir is not None:
            unchecked.append(f"{path} includes the directory {scan.include_dir}")
        if scan.substituted is not None:
            unchecked.append(
                f"{path} includes {scan.substituted} through a substitution"
            )

    def including(files: set[Path]) -> set[Path]:
        return {p for f in files for p in configs.named_by.get(f.resolve(), ())}

    def with_includers(users: set[Path]) -> set[Path]:
        frontier = users
        while frontier := including(frontier) - users:
            users = users | frontier
        return users

    def others(paths: set[Path]) -> list[Path]:
        return sorted(p for p in paths if p.resolve() not in own)

    for edit in edits:
        if edit.insert_after and edit.secret:
            continue  # a new secrets line has no users yet
        if edit.secret:
            users = _secret_users(edit.secret, edit.path)
        else:
            users = including({edit.path})
        edit.shared_with = others(with_includers(users))
    return unchecked


_HAND_EDIT = "only a key unique to this device can be rotated, change it by hand"


def rotation_edits(old_key: str, new_key: str) -> list[KeyEdit]:
    """Every edit of a rotation: the new key and the previous one as old_key.
    Refuses when another configuration uses or may use an edited line."""
    edits = [*locate_key_edits(old_key, new_key), *old_key_edit(old_key)]
    unchecked = with_sharers(edits)
    if shared := [e for e in edits if e.shared_with]:
        what = dict.fromkeys(
            f"secret '{e.secret}'" if e.secret else e.path.name for e in shared
        )
        root = CORE.config_dir.resolve()
        users = sorted(
            {str(p.relative_to(root)) for e in shared for p in e.shared_with}
        )
        raise EsphomeError(
            f"{', '.join(what)} {'is' if len(what) == 1 else 'are'} also used by "
            f"{', '.join(users)}; {_HAND_EDIT}"
        )
    if unchecked:
        more = f" (and {len(unchecked) - 1} more)" if len(unchecked) > 1 else ""
        raise EsphomeError(
            "Could not check whether other configurations use this key: "
            f"{unchecked[0]}{more}; {_HAND_EDIT}"
        )
    return edits


def _secret_line(
    secrets_path: Path, name: str, value: str | None = None, required: bool = False
) -> tuple[int, re.Match[str]] | None:
    """The root level ``name:`` line, None when absent unless ``required``."""
    lines = read_text(secrets_path).splitlines()
    line_re = field_line_re(name, value, re.escape(_root_indent(lines)))
    hit = next(
        ((i, m) for i, text in enumerate(lines) if (m := line_re.match(text))), None
    )
    if hit is None and required:
        holding = "" if value is None else " with the current key"
        raise EsphomeError(
            f"No plain '{name}:' line{holding} in {secrets_path}; edit it by hand"
        )
    return hit


def _has_root_key(secrets_path: Path, name: str) -> bool:
    """Whether a root level ``name:`` key exists, whatever its value."""
    lines = read_text(secrets_path).splitlines()
    key_re = re.compile(field_prefix(name, re.escape(_root_indent(lines))))
    return any(key_re.match(text) for text in lines)


def _secret_rewrite(
    secrets_path: Path,
    name: str,
    value: str,
    expect: str | None = None,
    required: bool = False,
) -> KeyEdit | None:
    """Set the root level ``name:`` line to ``value``; None when it is not a
    plain scalar holding ``expect``, unless ``required``."""
    if (hit := _secret_line(secrets_path, name, expect, required)) is None:
        return None
    i, m = hit
    return KeyEdit(secrets_path, i, m.string, rewrite(m, value), secret=name)


def _secret_insert(secrets_path: Path, after: str, name: str, value: str) -> KeyEdit:
    """Add ``name: value`` under the root level ``after`` line."""
    i, m = _secret_line(secrets_path, after, required=True)
    return KeyEdit(
        secrets_path,
        i,
        m.string,
        f'{_indent(m.string)}{name}: "{value}"',
        insert_after=True,
        secret=name,
    )


_SECRET_AS_LITERAL = (
    "The current key is kept in secrets.yaml, so it cannot go into the yaml as a "
    f"plain '{CONF_OLD_KEY}'; use a !secret for it or edit it by hand"
)


def _field_edits(
    blocks: list[ConfigType],
    option: str,
    new_value: str,
    expect: bool,
    literal_ok: bool,
) -> list[KeyEdit]:
    """Rewrite the ``option:`` line of each block, or the secret it names, to
    ``new_value``; with ``expect`` the secret must hold the block's value."""
    secret_re = secret_field_re(option)
    what = "the key" if option == CONF_KEY else f"'{option}'"
    edits: list[KeyEdit] = []
    seen: set[tuple[Path, int]] = set()
    allowed: dict[str, set[tuple[Path, int]]] = {}
    for block in blocks:
        doc, line_no, text = source_line(block, option)
        value = str(block[option])
        if match := secret_re.match(text):
            name = match["name"]
            edit = _secret_rewrite(
                editable_file(_secrets_path(doc)),
                name,
                new_value,
                expect=value if expect else None,
                required=True,
            )
            allowed.setdefault(name, set()).add((doc.resolve(), line_no))
        elif match := field_line_re(option, value).match(text):
            if not literal_ok:
                raise EsphomeError(_SECRET_AS_LITERAL)
            edit = KeyEdit(doc, line_no, text, rewrite(match, new_value))
        else:
            raise EsphomeError(
                f"{doc}:{line_no + 1} does not hold {what} as a plain value or a "
                f"!secret; edit {what} by hand"
            )
        # The api and ota blocks may share one !secret line
        if (where := (edit.path.resolve(), edit.line)) not in seen:
            seen.add(where)
            edits.append(edit)
    for name, lines in allowed.items():
        _refuse_other_own_uses(name, lines)
    return edits


def locate_key_edits(old_key: str, new_key: str) -> list[KeyEdit]:
    """The edits that replace the current key; refuses a substitution, flow
    mapping or remote package."""
    if not (blocks := _key_blocks(CORE.raw_config or {}, old_key)):
        raise EsphomeError("The current key was not found on a line of the yaml")
    return _field_edits(blocks, CONF_KEY, new_key, expect=True, literal_ok=True)


def _refuse_other_own_uses(
    name: str, allowed: Collection[tuple[Path, int]] = ()
) -> None:
    """The secret may only feed this config's ``allowed`` lines; a wifi
    password on it would change too."""
    for doc in own_documents():
        for secret, i in _scan(doc).secrets:
            if secret == name and (doc, i) not in allowed:
                raise EsphomeError(
                    f"'{name}' is also used at {doc}:{i + 1}; a rotation would "
                    "change that value too, edit the key by hand"
                )


def _secret_name(block: ConfigType) -> str | None:
    """The secrets file name a ``key: !secret <name>`` line of ``block`` holds."""
    if source_of(block, CONF_KEY) is None:
        return None
    match = _KEY_SECRET_RE.match(source_line(block, CONF_KEY)[2])
    return match["name"] if match else None


def old_key_edit(old_key: str) -> list[KeyEdit]:
    """Set ``old_key:`` on the esphome ota block, rewriting or adding it. A
    key from secrets.yaml stays there as ``<name>_old``. Of several entries
    the one with its own key line gets it."""
    raw = CORE.raw_config or {}
    items = [item for item in esphome_ota_items(raw) if CONF_ENCRYPTION in item]
    if not items:
        raise EsphomeError("The esphome OTA platform has no 'encryption:' block")
    encs = [item[CONF_ENCRYPTION] or {} for item in items]
    api_enc = (raw.get(CONF_API) or {}).get(CONF_ENCRYPTION) or {}
    # Only a key that never appears in the yaml itself must stay out of it
    key_blocks = _key_blocks(raw, old_key)
    secret_backed = bool(key_blocks) and all(map(_secret_name, key_blocks))
    # Same-port entries must agree, so every existing old_key is rewritten
    if with_old := [enc for enc in encs if enc.get(CONF_OLD_KEY) is not None]:
        return _field_edits(
            with_old, CONF_OLD_KEY, old_key, expect=False, literal_ok=not secret_backed
        )
    if keyed := next((enc for enc in encs if source_of(enc, CONF_KEY)), None):
        doc, line_no, text = source_line(keyed, CONF_KEY)
        anchor, anchor_re, indent = CONF_KEY, _KEY_FIELD_RE, _indent(text)
        if (api_key := source_of(api_enc, CONF_KEY)) and (
            api_key[0].resolve() == doc.resolve() and api_key[1] == line_no
        ):
            # The api encryption schema takes no old_key
            raise EsphomeError(
                f"{doc}:{line_no + 1} is the api key too; add '{CONF_OLD_KEY}' to "
                "an ota-only encryption block by hand"
            )
    else:
        # A bare block takes after the api key
        keyed = api_enc
        # Package entries merge in first, so the last one is the device's own
        doc, line_no, text = source_line(items[-1], CONF_ENCRYPTION)
        anchor, anchor_re = CONF_ENCRYPTION, _BARE_BLOCK_RE
        indent = _child_indent(doc, line_no, _indent(text))
    if not anchor_re.match(text):
        raise EsphomeError(
            f"{doc}:{line_no + 1} does not hold '{anchor}:' as a block line; "
            "edit it by hand"
        )
    if (secret := _secret_name(keyed)) is None:
        if secret_backed:
            raise EsphomeError(_SECRET_AS_LITERAL)
        rendered = f'{CONF_OLD_KEY}: "{old_key}"'
        extra = []
    else:
        rendered = f"{CONF_OLD_KEY}: !secret {secret}_old"
        secrets_path = editable_file(_secrets_path(doc))
        name = f"{secret}_old"
        # A leftover `<secret>_old` line is reused; rotation_edits refuses
        # it when another configuration uses it
        if (kept := _secret_rewrite(secrets_path, name, old_key)) is not None:
            _refuse_other_own_uses(name)
        elif _has_root_key(secrets_path, name):  # a second one would not load
            raise EsphomeError(
                f"'{name}:' in {secrets_path} is not a plain line; edit it by hand"
            )
        extra = [kept or _secret_insert(secrets_path, secret, name, old_key)]
    edit = KeyEdit(doc, line_no, text, f"{indent}{rendered}", insert_after=True)
    return [edit, *extra]
