from __future__ import annotations

from collections.abc import Generator
from dataclasses import dataclass
import logging
import os
from pathlib import Path
import struct

import yaml

from esphome import yaml_util
import esphome.codegen as cg
from esphome.components import packages
from esphome.components.api import CONF_ENCRYPTION
from esphome.config_helpers import Extend, Remove
import esphome.config_validation as cv
from esphome.const import (
    CONF_API,
    CONF_DEFAULTS,
    CONF_KEY,
    CONF_PACKAGES,
    CONF_PASSWORD,
    CONF_SUBSTITUTIONS,
)
from esphome.core import CORE, EsphomeError, Lambda
from esphome.expression import SUBSTITUTION_VARIABLE_PROG
import esphome.final_validate as fv
from esphome.git import URL_USERINFO_RE, redact_url_credentials
from esphome.helpers import ensure_unique_string, zstd_module
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@bdraco"]
DEPENDENCIES = ["api"]
DOMAIN = "store_yaml"

CONF_INCLUDE_SECRETS = "include_secrets"
CONF_ALLOW_UNENCRYPTED = "allow_unencrypted"


# Compression level for zstd; 22 is the max and gives ~70-90% reduction on YAML.
ZSTD_LEVEL = 22
# Envelope magic: "EHY1" = ESPHome YAML, version 1.
ENVELOPE_MAGIC = b"EHY1"
# Replacement content for secrets files: a fill-in skeleton listing every
# `!secret` key the recovered config needs.
SECRETS_SKELETON_HEADER = (
    "# Redacted by store_yaml. Fill in these values and the recovered\n"
    "# config is ready to flash.\n"
)
# Envelope path of the note recording content that could not be captured.
UNCAPTURED_NOTE_PATH = "store_yaml_uncaptured.yaml"
# Substitution names to every value they may take in one file.
_Scope = dict[str, list[object]]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_INCLUDE_SECRETS, default=False): cv.boolean,
        cv.Optional(CONF_ALLOW_UNENCRYPTED, default=False): cv.boolean,
    }
)


def _final_validate(config: ConfigType) -> ConfigType:
    """Require an API encryption key unless ``allow_unencrypted`` is set."""
    full = fv.full_config.get()
    api_conf = full.get(CONF_API, {})
    # A keyless `encryption:` accepts the all-zeros PSK until provisioned
    encryption = api_conf.get(CONF_ENCRYPTION) or {}
    if CONF_KEY in encryption:
        return config
    if config.get(CONF_ALLOW_UNENCRYPTED):
        _LOGGER.warning(
            "store_yaml is enabled without API encryption; any client that can "
            "reach the device on the network can pull the embedded YAML%s.",
            " including the verbatim contents of secrets.yaml"
            if config.get(CONF_INCLUDE_SECRETS)
            else "",
        )
        return config
    raise cv.Invalid(
        "store_yaml requires API encryption (configure `api.encryption.key`). "
        "Without encryption, the embedded YAML — which may contain Wi-Fi "
        "credentials or opted-in secrets — can be read by any client that "
        "reaches the device. Set `store_yaml.allow_unencrypted: true` to "
        "override after acknowledging the risk."
    )


FINAL_VALIDATE_SCHEMA = _final_validate


def _gather_files(
    discovered: yaml_util.DiscoveredYamlFiles,
) -> tuple[list[tuple[str, Path]], set[str]]:
    """Map each discovered YAML file to its envelope path.

    Returns (relative_path, source_path) pairs, one per name the YAML reached
    the file by, and the relative paths of secrets files.
    """
    if not discovered.files:
        raise EsphomeError(
            "store_yaml could not discover any YAML files for "
            f"{CORE.config_path}; nothing to embed."
        )

    if discovered.load_errors:
        # A silently partial recovery blob defeats the feature; fail the build
        # instead of embedding an incomplete file set.
        raise EsphomeError(
            "store_yaml: could not load all configuration files: "
            + "; ".join(discovered.load_errors)
        )

    if discovered.unresolved:
        _LOGGER.warning(
            "store_yaml: %d !include path(s) use substitutions and cannot be "
            "captured (%s); the embedded recovery data will not contain them",
            len(discovered.unresolved),
            ", ".join(discovered.unresolved),
        )

    # Aliases are lexical paths, so they are placed against the lexical root.
    root = Path(os.path.normpath(CORE.config_path.absolute())).parent

    entries: list[tuple[str, Path]] = []
    secret_rels: set[str] = set()
    for path in discovered.files:
        for alias in discovered.aliases.get(path, [path]):
            # Keep ".." for files outside the root so same-named files don't collide.
            try:
                rel_str = alias.relative_to(root, walk_up=True).as_posix()
            except ValueError as err:
                # No common anchor, e.g. a Windows file on another drive.
                raise EsphomeError(
                    f"store_yaml: cannot place {alias} in the recovery envelope; "
                    f"it does not share a root with {root}: {err}"
                ) from err
            if path in discovered.secrets:
                secret_rels.add(rel_str)
            entries.append((rel_str, path))

    return entries, secret_rels


def _read_files_verbatim(entries: list[tuple[str, Path]]) -> list[tuple[str, bytes]]:
    """Read each file's exact on-disk bytes (the `include_secrets: true` path)."""
    files: list[tuple[str, bytes]] = []
    for rel, path in entries:
        try:
            files.append((rel, path.read_bytes()))
        except OSError as err:
            raise EsphomeError(
                f"store_yaml: cannot read tracked YAML file {path}: {err}"
            ) from err
    return files


def _iter_nodes(
    node: object, path: tuple[str, ...] = ()
) -> Generator[tuple[tuple[str, ...], object, bool]]:
    """Yield (config_path, value, is_key) for every key and scalar in a tree.

    Wrappers the dumper writes as text (`!lambda`, `!extend`, `!remove`,
    `!include` paths and vars) are unwrapped so their payloads are scanned.
    """
    if isinstance(node, dict):
        for key, value in node.items():
            yield path, str(key), True
            yield from _iter_nodes(value, (*path, str(key)))
    elif isinstance(node, (list, tuple)):
        for item in node:
            yield from _iter_nodes(item, path)
    elif isinstance(node, (Lambda, Extend, Remove)):
        yield path, node.value, False
    elif isinstance(node, yaml_util.IncludeFile):
        yield path, str(node.file), False
        if isinstance(node.condition, str):
            yield path, node.condition, False
        if node.vars:
            yield from _iter_nodes(node.vars, path)
    elif isinstance(node, (str, int, float)) and not isinstance(node, bool):
        yield path, node, False


@dataclass
class _SensitiveValue:
    secret_name: str
    config_path: str  # dotted path, for warnings (never log the value itself)
    from_secret: bool  # loaded through a real `!secret`
    # Last path segments the value is sensitive at (e.g. {"password"}), used to
    # tell the value's own occurrences apart from unrelated collisions.
    sensitive_keys: set[str]
    # Integers skip the substring scan; short PINs would match unrelated text.
    is_int: bool = False


def _iter_includes(node: object) -> Generator[yaml_util.IncludeFile]:
    """Yield every `!include` in a tree, including those inside include vars."""
    if isinstance(node, dict):
        for value in node.values():
            yield from _iter_includes(value)
    elif isinstance(node, list):
        for item in node:
            yield from _iter_includes(item)
    elif isinstance(node, yaml_util.IncludeFile):
        yield node
        if node.vars:
            yield from _iter_includes(node.vars)


def _include_scopes(
    trees: dict[str, object], sources: dict[str, Path]
) -> dict[str, _Scope]:
    """Every value a substitution name may take in each file: top-level
    substitutions, the file's `defaults:` and the vars of every `!include` of it."""

    def mapping(tree: object, key: str) -> dict:
        value = tree.get(key) if isinstance(tree, dict) else None
        return value if isinstance(value, dict) else {}

    def add(scope: _Scope, values: dict) -> None:
        for name, value in values.items():
            scope.setdefault(str(name), []).append(value)

    rels_by_path: dict[Path, list[str]] = {}
    for rel, path in sources.items():
        rels_by_path.setdefault(path, []).append(rel)
    scopes: dict[str, _Scope] = {rel: {} for rel in trees}
    for rel, tree in trees.items():
        for scope in scopes.values():
            add(scope, mapping(tree, CONF_SUBSTITUTIONS))
        add(scopes[rel], mapping(tree, CONF_DEFAULTS))
        for include in _iter_includes(tree):
            if not include.vars or include.has_unresolved_file():
                continue
            target = (include.parent_file.parent / include.file).resolve()
            for target_rel in rels_by_path.get(target, ()):
                add(scopes[target_rel], include.vars)
    return scopes


def _reference_names(text: str) -> list[str]:
    """Substitution names referenced as `$name` or `${name}` in `text`."""
    return [
        m[1:-1] if m.startswith("{") else m
        for m in SUBSTITUTION_VARIABLE_PROG.findall(text)
    ]


def _check_sensitive_usage(
    sensitive: dict[str, _SensitiveValue],
    trees: dict[str, object],
    scopes: dict[str, _Scope],
) -> None:
    """Check where sensitive values occur beyond their own scalars.

    Mapping key: fail. Inside a larger scalar: fail if inline, warn if `!secret`.
    Whole scalar elsewhere: warn, it is recovered as the `!secret` reference.
    A value at a credential location that validation never saw, or a URL with
    userinfo, fails unless it (or every definition of the substitution it
    references) is a `!secret` or a known sensitive value.
    """
    sensitive_paths = {
        tuple(info.config_path.split(".")) for info in sensitive.values()
    }

    def verified(text: str) -> bool:
        return not text or text in sensitive or yaml_util.is_secret(text) is not None

    def references_verified(text: str, scope: _Scope) -> bool:
        names = _reference_names(text)
        return bool(names) and all(
            name in scope and all(verified(str(value)) for value in scope[name])
            for name in names
        )

    def credential_path(path: tuple[str, ...]) -> bool:
        """Whether `path` lands where the validated config holds a credential.

        A git package password never reaches validation. An inline package
        merges at the top level, so its `packages.<name>` prefix goes; an
        included file's path is a suffix of the validated one; and validation
        regroups keys within a domain (`wifi.password` to `wifi.networks.password`).
        """
        if not path:
            return False
        if path[0] == CONF_PACKAGES:
            if path[-1] == CONF_PASSWORD:
                return True
            path = path[1:]
            if path and path[0] not in packages.REMOTE_PACKAGE_KEYS:
                path = path[1:]
        return bool(path) and any(
            sp[-len(path) :] == path
            or (len(path) >= 2 and path[0] == sp[0] and path[-1] == sp[-1])
            for sp in sensitive_paths
        )

    def shown(path: tuple[str, ...]) -> str:
        """`path` for messages, with a segment that is a credential masked."""
        return ".".join(
            f"<!secret {sensitive[seg].secret_name}>" if seg in sensitive else seg
            for seg in path
        )

    key_hits: list[str] = []
    embedded: list[str] = []
    unverified: list[str] = []
    for rel, tree in trees.items():
        scope = scopes[rel]
        for path, node, is_key in _iter_nodes(tree):
            text = str(node)
            if (info := sensitive.get(text)) is not None:
                if is_key:
                    key_hits.append(
                        f"{info.config_path} (as a mapping key under "
                        f"{shown(path) or 'the document root'} in {rel})"
                    )
                elif not path or (
                    path[0] != CONF_SUBSTITUTIONS
                    and path[-1] not in info.sensitive_keys
                ):
                    _LOGGER.warning(
                        "store_yaml: the sensitive value at %s also matches the "
                        "scalar at %s in %s; the recovered config will reference "
                        "!secret %s there too",
                        info.config_path,
                        shown(path),
                        rel,
                        info.secret_name,
                    )
                continue
            if is_key or verified(text):
                continue
            if any(
                SUBSTITUTION_VARIABLE_PROG.sub("", userinfo) not in ("", ":")
                or not references_verified(userinfo, scope)
                for userinfo in URL_USERINFO_RE.findall(text)
            ):
                unverified.append(f"{shown(path)} in {rel} (a URL with credentials)")
            elif credential_path(path) and not (
                # Only a whole-scalar reference can be traced; an expression cannot.
                SUBSTITUTION_VARIABLE_PROG.fullmatch(text) is not None
                and references_verified(text, scope)
            ):
                unverified.append(f"{shown(path)} in {rel}")
                continue
            for value, other in sensitive.items():
                if other.is_int or value not in text:
                    continue
                if other.from_secret:
                    # Already a real !secret and overlaps (an SSID in a name)
                    # are common, so warn without the value instead of failing.
                    _LOGGER.warning(
                        "store_yaml: the value of !secret %s (sensitive at %s) "
                        "appears inside the value at %s in %s; substring "
                        "occurrences are not redacted",
                        other.secret_name,
                        other.config_path,
                        shown(path),
                        rel,
                    )
                else:
                    embedded.append(
                        f"{other.config_path} (inside {shown(path)} in {rel})"
                    )
    if key_hits:
        raise EsphomeError(
            "store_yaml: sensitive value(s) are also used as mapping keys: "
            f"{', '.join(key_hits)}. The redaction swap would rewrite the key "
            "and corrupt the recovered config. Change the value, or set "
            "`include_secrets: true` to embed secrets deliberately."
        )
    if embedded:
        raise EsphomeError(
            "store_yaml: sensitive value(s) appear embedded inside larger "
            f"values: {', '.join(embedded)}. Redaction only replaces whole "
            "scalars, so these would ship unredacted. Move the value into a "
            "`!secret` referenced on its own, or set `include_secrets: true` "
            "to embed secrets deliberately."
        )
    if unverified:
        raise EsphomeError(
            "store_yaml: cannot confirm that these values are redacted: "
            f"{', '.join(unverified)}. They sit where credentials go but are "
            "not in the validated config (overridden by a later package, or a "
            "git package password that is used before validation). Reference "
            "them with `!secret`, or set `include_secrets: true` to embed "
            "secrets deliberately."
        )


def _collect_sensitive_values() -> dict[str, _SensitiveValue]:
    """Map each cv.sensitive value to the `!secret` name it is recovered as.

    `!secret` values keep their name; inline values get one from their config
    path that does not clash with real secrets.
    """
    used = yaml_util.registered_secret_names()
    result: dict[str, _SensitiveValue] = {}
    for path, node, is_key in _iter_nodes(CORE.config):
        if is_key or not isinstance(node, yaml_util.Sensitive) or not node:
            continue
        # Decimal for ints, to match the raw tree's parsed scalars, not HexInt's hex.
        value = str(int(node)) if isinstance(node, int) else str(node)
        entry = result.get(value)
        if entry is None:
            name = yaml_util.is_secret(value)
            from_secret = name is not None
            if name is None:
                name = ensure_unique_string("_".join(path) or "secret", used)
            used.add(name)
            entry = result[value] = _SensitiveValue(
                name, ".".join(path), from_secret, set(), isinstance(node, int)
            )
        if path:
            entry.sensitive_keys.add(path[-1])
    return result


def _uncaptured_note(
    unresolved: list[str], remote_packages: list[str]
) -> tuple[str, bytes] | None:
    """Return a comment-only entry listing uncaptured content, or None if there is none."""
    if not unresolved and not remote_packages:
        return None
    parts = ["# store_yaml: the following content could not be captured.\n"]
    if unresolved:
        parts.append(
            "# These !include paths use substitutions; restore the files manually:\n"
            + "".join(f"#   {inc}\n" for inc in unresolved)
        )
    if remote_packages:
        parts.append(
            "# These packages come from remote sources; re-fetch them to\n"
            "# complete this config:\n"
            + "".join(f"#   {pkg}\n" for pkg in remote_packages)
        )
    return (UNCAPTURED_NOTE_PATH, "".join(parts).encode("utf-8"))


def _describe_url(url: str) -> str:
    """A URL for notes and logs: its secret name, or the URL with userinfo masked."""
    if (name := yaml_util.is_secret(url)) is not None:
        return f"!secret {name}"
    return redact_url_credentials(url)


def _remote_package_descriptions() -> list[str]:
    """Describe every remote source packages were fetched from; their files cannot be embedded."""
    return [
        f"{_describe_url(source.url)}@{source.ref}"
        if source.ref
        else _describe_url(source.url)
        for source in packages.get_remote_package_sources()
    ]


def _build_secrets_skeleton(keys: set[str]) -> bytes:
    # Plain PyYAML quotes a key such as "true" and does no secret substitution.
    body = yaml.safe_dump(dict.fromkeys(sorted(keys), ""), sort_keys=False)
    return (SECRETS_SKELETON_HEADER + (body if keys else "")).encode("utf-8")


def _prepare_tree(node: object, source: Path, fold_secrets: bool) -> object:
    """Make a loaded file's tree dump as the source it came from.

    A mapping or list loaded through `!secret` becomes its registered text, which
    the dumper writes as the reference (a plain str would skip the secret check).
    An `!include` pulled in by `!include_dir_*` is rebased onto `source`.
    """
    if (
        fold_secrets
        and isinstance(node, (dict, list))
        and yaml_util.is_secret(node) is not None
    ):
        return yaml_util.make_data_base(str(node))
    if isinstance(node, dict):
        for key, value in node.items():
            node[key] = _prepare_tree(value, source, fold_secrets)
    elif isinstance(node, list):
        for i, item in enumerate(node):
            node[i] = _prepare_tree(item, source, fold_secrets)
    elif isinstance(node, yaml_util.IncludeFile):
        if node.vars:
            node.vars = _prepare_tree(node.vars, source, fold_secrets)
        origin = node.parent_file.resolve()
        if origin != source and not node.has_unresolved_file():
            return yaml_util.IncludeFile(
                source,
                Path(
                    os.path.relpath(origin.parent / node.file, source.parent)
                ).as_posix(),
                node.yaml_loader,
                vars=node.vars,
                condition=node.condition,
            )
    return node


def _generate_redacted_files(
    entries: list[tuple[str, Path]], secret_rels: set[str], remote_packages: list[str]
) -> list[tuple[str, bytes]]:
    """Re-dump captured files with sensitive values as `!secret` and secrets files as a skeleton.

    Comments and formatting are not preserved.
    """
    sensitive = _collect_sensitive_values()

    captured = {rel: path for rel, path in entries if rel not in secret_rels}
    # A document that is one `!include` keeps its vars and condition.
    loaded = {
        rel: yaml_util.load_yaml(
            path, clear_secrets=False, resolve_document_include=False
        )
        for rel, path in captured.items()
    }
    fold_secrets = yaml_util.has_container_secrets()
    trees = {
        rel: _prepare_tree(tree, captured[rel], fold_secrets)
        for rel, tree in loaded.items()
    }
    _check_sensitive_usage(sensitive, trees, _include_scopes(trees, captured))

    registered = {value: info.secret_name for value, info in sensitive.items()}
    with yaml_util.secret_values_registered(registered) as skeleton_keys:
        texts = {rel: yaml_util.dump(tree) for rel, tree in trees.items()}

    # An inline value whose name was never emitted was not found as a whole
    # scalar, so its redaction cannot be verified.
    leaked = [
        info.config_path
        for info in sensitive.values()
        if not info.from_secret and info.secret_name not in skeleton_keys
    ]
    if leaked:
        raise EsphomeError(
            "store_yaml: could not redact the sensitive value(s) of "
            f"{', '.join(leaked)}. The value was not found in any captured "
            "file; it may be composed via substitutions, set on the command "
            "line with -s, or defined inside a remote package"
            + (f" ({', '.join(remote_packages)})" if remote_packages else "")
            + ". Reference it with `!secret` in the YAML, or set "
            "`include_secrets: true` to embed secrets deliberately."
        )

    skeleton = _build_secrets_skeleton(skeleton_keys)
    result = [
        (rel, skeleton if rel in secret_rels else texts[rel].encode("utf-8"))
        for rel, _ in entries
    ]
    if skeleton_keys and yaml_util.SECRET_YAML not in secret_rels:
        # No captured secrets file lands at the config root, but `!secret` looks
        # for secrets.yaml beside the config; ship a skeleton so it loads.
        result.append((yaml_util.SECRET_YAML, skeleton))
    return result


def _pack_envelope(files: list[tuple[str, bytes]]) -> bytes:
    """Pack files into the EHY1 envelope (little-endian).

    magic (4) | u32 count | { u16 path_len | path | u32 content_len | content }...
    """
    parts: list[bytes] = [ENVELOPE_MAGIC, struct.pack("<I", len(files))]
    seen: set[str] = set()
    for path, content in files:
        if path in seen:
            # unpack_envelope would silently keep only the last duplicate.
            raise EsphomeError(f"store_yaml: duplicate envelope path: {path}")
        seen.add(path)
        path_bytes = path.encode("utf-8")
        if len(path_bytes) > 0xFFFF:
            raise EsphomeError(
                f"store_yaml: path too long ({len(path_bytes)} bytes): {path}"
            )
        parts.append(struct.pack("<H", len(path_bytes)))
        parts.append(path_bytes)
        parts.append(struct.pack("<I", len(content)))
        parts.append(content)
    return b"".join(parts)


def unpack_envelope(blob: bytes) -> dict[str, bytes]:
    """Reference decoder for the EHY1 envelope.

    Absolute paths are rejected but ``..`` is allowed; callers writing to disk must confine paths.
    """
    if blob[:4] != ENVELOPE_MAGIC:
        raise EsphomeError("envelope must start with EHY1 magic")
    pos = 4
    files: dict[str, bytes] = {}

    def take(n: int) -> bytes:
        nonlocal pos
        if pos + n > len(blob):
            raise EsphomeError("truncated envelope")
        chunk = blob[pos : pos + n]
        pos += n
        return chunk

    (count,) = struct.unpack("<I", take(4))
    for _ in range(count):
        (path_len,) = struct.unpack("<H", take(2))
        try:
            path = take(path_len).decode("utf-8")
        except UnicodeDecodeError as err:
            raise EsphomeError(f"envelope path is not valid UTF-8: {err}") from err
        if path.startswith(("/", "\\")) or (len(path) >= 2 and path[1] == ":"):
            raise EsphomeError(f"envelope contains non-relative path: {path}")
        if path in files:
            raise EsphomeError(f"envelope contains duplicate path: {path}")
        (content_len,) = struct.unpack("<I", take(4))
        files[path] = take(content_len)
    if pos != len(blob):
        raise EsphomeError("envelope has trailing bytes")
    return files


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_STORE_YAML")

    # Re-parse to capture only user files; also refills the secret registry redaction needs.
    discovered = yaml_util.discover_user_yaml_files(CORE.config_path)
    entries, secret_rels = _gather_files(discovered)
    remote_packages = _remote_package_descriptions()
    if config[CONF_INCLUDE_SECRETS]:
        files = _read_files_verbatim(entries)
    else:
        files = _generate_redacted_files(entries, secret_rels, remote_packages)
    if remote_packages:
        _LOGGER.warning(
            "store_yaml: %d package(s) come from remote sources and cannot be "
            "captured (%s); the embedded recovery data records the source so "
            "they can be re-fetched",
            len(remote_packages),
            ", ".join(remote_packages),
        )
    if (note := _uncaptured_note(discovered.unresolved, remote_packages)) is not None:
        files.append(note)
    envelope = _pack_envelope(files)
    compressed = zstd_module().compress(envelope, level=ZSTD_LEVEL)

    _LOGGER.info(
        "store_yaml: embedding %d file(s) as %d bytes (%d uncompressed, %.1f%% ratio)",
        len(files),
        len(compressed),
        len(envelope),
        100.0 * len(compressed) / len(envelope),
    )

    cg.extern_progmem_array(
        "esphome::store_yaml::STORE_YAML_DATA", cg.uint8, list(compressed)
    )
    cg.add_define("STORE_YAML_DATA_SIZE", len(compressed))
