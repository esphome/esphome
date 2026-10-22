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
from esphome.components import noise, packages
from esphome.config_helpers import Extend, Remove
import esphome.config_validation as cv
from esphome.const import (
    CONF_API,
    CONF_DEFAULTS,
    CONF_PACKAGES,
    CONF_PASSWORD,
    CONF_SUBSTITUTIONS,
)
from esphome.core import CORE, EsphomeError, Lambda
from esphome.expression import SUBSTITUTION_VARIABLE_PROG
import esphome.final_validate as fv
from esphome.git import URL_USERINFO_RE
from esphome.helpers import ensure_unique_string, zstd_module
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@bdraco"]
DEPENDENCIES = ["api"]
DOMAIN = "store_yaml"

CONF_INCLUDE_SECRETS = "include_secrets"
CONF_ALLOW_UNENCRYPTED = "allow_unencrypted"

# 22 is the max and gives ~70-90% reduction on YAML.
ZSTD_LEVEL = 22
# "EHY1" = ESPHome YAML, version 1.
ENVELOPE_MAGIC = b"EHY1"
SECRETS_SKELETON_HEADER = (
    "# Redacted by store_yaml. Fill in these values and the recovered\n"
    "# config is ready to flash.\n"
)
UNCAPTURED_NOTE_PATH = "store_yaml_uncaptured.yaml"
_FIX = (
    "Reference them with `!secret`, or set `include_secrets: true` to embed "
    "secrets deliberately."
)
# Substitution names to every value they may take in one file.
_Scope = dict[str, list[object]]
# (named path of the file holding it, the include, its vars before any folding)
# (named path of the file holding it, the include, its vars before any folding,
# the config path of the include site)
_Include = tuple[Path, yaml_util.IncludeFile, dict[str, object], tuple[str, ...]]
# Stands in for an integer credential, which is swapped only at its own keys.
_INT_MARK = "\x00"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_INCLUDE_SECRETS, default=False): cv.boolean,
        cv.Optional(CONF_ALLOW_UNENCRYPTED, default=False): cv.boolean,
    }
)


def _final_validate(config: ConfigType) -> ConfigType:
    """Require an API encryption key unless ``allow_unencrypted`` is set."""
    full = fv.full_config.get()
    if noise.static_encryption_key(full.get(CONF_API, {})) is not None:
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
        "store_yaml requires API encryption (`api.encryption.key`), or any client "
        "that reaches the device can read the embedded YAML. Set "
        "`store_yaml.allow_unencrypted: true` to accept that."
    )


FINAL_VALIDATE_SCHEMA = _final_validate


def _gather_files(
    discovered: yaml_util.DiscoveredYamlFiles,
) -> tuple[list[tuple[str, Path]], set[str]]:
    """(envelope path, named path) per name a file was reached by, and the envelope
    paths of secrets files."""
    if discovered.load_errors:
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

    root = yaml_util.named_path(CORE.config_path).parent
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
            entries.append((rel_str, alias))
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
    """Yield (config_path, value, is_key) for every key and scalar, unwrapping the
    tagged wrappers the dumper writes as text."""
    if isinstance(node, dict):
        for key, value in node.items():
            yield path, str(key), True
            yield from _iter_nodes(value, (*path, str(key)))
    elif isinstance(node, (list, tuple)):
        for item in node:
            yield from _iter_nodes(item, path)
    elif isinstance(node, Remove):
        yield path, node, False
    elif isinstance(node, (Lambda, Extend)):
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
    config_path: str  # dotted path, for messages (never the value itself)
    from_secret: bool  # loaded through a real `!secret`
    # Keys the value sits at, to tell its own occurrences from collisions.
    sensitive_keys: set[str]
    # Integers skip the substring scan; short PINs would match unrelated text.
    is_int: bool = False


def _include_scopes(
    includes: list[_Include], trees: dict[str, object], sources: dict[str, Path]
) -> tuple[dict[str, _Scope], dict[str, list[tuple[str, ...]]]]:
    """Per file: every value a substitution name may take (per include: its vars,
    else the file's `defaults:`, else the caller's chain, else substitutions) and
    the config paths its content lands under."""

    def mapping(tree: object, key: str) -> dict[str, object]:
        value = tree.get(key) if isinstance(tree, dict) else None
        return {str(k): v for k, v in value.items()} if isinstance(value, dict) else {}

    substitutions: _Scope = {}
    for tree in trees.values():
        for name, value in mapping(tree, CONF_SUBSTITUTIONS).items():
            substitutions.setdefault(name, []).append(value)
    defaults = {rel: mapping(tree, CONF_DEFAULTS) for rel, tree in trees.items()}
    rels_by_path: dict[Path, list[str]] = {}
    for rel, path in sources.items():
        rels_by_path.setdefault(path.resolve(), []).append(rel)
    edges: dict[str, list[tuple[str, dict[str, object], tuple[str, ...]]]] = {
        rel: [] for rel in trees
    }
    for holder, include, vars, site in includes:
        if include.has_unresolved_file():
            continue
        target = (include.parent_file.parent / include.file).resolve()
        for src in rels_by_path.get(holder.resolve(), ()):
            edges[src].extend((rel, vars, site) for rel in rels_by_path.get(target, ()))
    # Each file's invocations: the chain seen inside it, as the loader builds it
    # (that include's vars, the file's own defaults, then the caller's chain).
    # With the config path the file's content lands under, so an included file's
    # own paths can be checked at their real location.
    targeted = {rel for targets in edges.values() for rel, _, _ in targets}
    invocations = {
        rel: [] if rel in targeted else [(defaults[rel], ())] for rel in trees
    }
    pending = [rel for rel in trees if rel not in targeted]
    while pending:
        src = pending.pop()
        for rel, vars, site in edges[src]:
            for context, prefix in invocations[src]:
                invocation = ({**context, **defaults[rel], **vars}, prefix + site)
                if invocation not in invocations[rel]:
                    invocations[rel].append(invocation)
                    pending.append(rel)
    scopes: dict[str, _Scope] = {}
    prefixes: dict[str, list[tuple[str, ...]]] = {}
    for rel in trees:
        scope: _Scope = {}
        for chain, _ in invocations[rel] or [(defaults[rel], ())]:
            for name in {*substitutions, *chain}:
                values = scope.setdefault(name, [])
                if name in chain:
                    values.append(chain[name])
                else:
                    values.extend(substitutions[name])
        scopes[rel] = scope
        prefixes[rel] = [prefix for _, prefix in invocations[rel]] or [()]
    return scopes, prefixes


def _check_sensitive_usage(
    sensitive: dict[str, _SensitiveValue],
    sensitive_paths: set[tuple[str, ...]],
    trees: dict[str, object],
    scopes: dict[str, _Scope],
    prefixes: dict[str, list[tuple[str, ...]]],
) -> None:
    """Fail on a sensitive value used as a key or inside a larger scalar, warn on an
    unrelated whole-scalar match, and fail on an unverifiable value at a credential
    location (also under a `!remove`d subtree) or in URL userinfo."""
    removed = {
        prefix + path
        for rel, tree in trees.items()
        for path, node, _ in _iter_nodes(tree)
        if isinstance(node, Remove)
        for prefix in prefixes[rel]
    }
    credential_keys = {sp[-1] for sp in sensitive_paths if sp}

    def verified(text: str) -> bool:
        return not text or text in sensitive or yaml_util.is_secret(text) is not None

    def references_verified(text: str, scope: _Scope) -> bool:
        names = [
            m[1:-1] if m.startswith("{") else m
            for m in SUBSTITUTION_VARIABLE_PROG.findall(text)
        ]
        return bool(names) and all(
            name in scope and all(verified(str(value)) for value in scope[name])
            for name in names
        )

    def ends_with(full: tuple[str, ...], part: tuple[str, ...]) -> bool:
        """`part` ends `full`, skipping segments validation inserted (`wifi.password`
        to `wifi.networks.password`)."""
        if not part or full[-1] != part[-1]:
            return False
        rest = iter(reversed(full[:-1]))
        return all(seg in rest for seg in reversed(part[:-1]))

    def merged(path: tuple[str, ...]) -> set[tuple[str, ...]]:
        """`path` as validation sees it: every `packages[.<name>]` prefix merges away."""
        if path and path[0] == CONF_PACKAGES:
            return merged(path[1:]) | merged(path[2:])
        return {path}

    def credential_path(path: tuple[str, ...]) -> bool:
        """Whether `path` lands where the validated config holds a credential."""
        if path and path[0] == CONF_PACKAGES and path[-1] == CONF_PASSWORD:
            return True
        return any(
            p
            and (
                (p[-1] in credential_keys and any(p[: len(r)] == r for r in removed))
                or any(ends_with(sp, p) for sp in sensitive_paths)
            )
            for p in merged(path)
        )

    def shown(path: tuple[str, ...]) -> str:
        """`path` for messages, credential segments masked."""
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
            if isinstance(node, Remove):
                continue
            text = str(node)
            if (info := sensitive.get(text)) is not None:
                if is_key:
                    key_hits.append(
                        f"{info.config_path} (as a mapping key under "
                        f"{shown(path) or 'the document root'} in {rel})"
                    )
                elif not info.is_int and (
                    not path
                    or (
                        path[0] != CONF_SUBSTITUTIONS
                        and path[-1] not in info.sensitive_keys
                    )
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
            if verified(text):
                continue
            if not is_key and any(
                SUBSTITUTION_VARIABLE_PROG.sub("", userinfo) not in ("", ":")
                or not references_verified(userinfo, scope)
                for userinfo in URL_USERINFO_RE.findall(text)
            ):
                unverified.append(f"{shown(path)} in {rel} (a URL with credentials)")
            elif (
                not is_key
                and any(credential_path(prefix + path) for prefix in prefixes[rel])
                and not (
                    # Only a whole-scalar reference can be traced; an expression cannot.
                    SUBSTITUTION_VARIABLE_PROG.fullmatch(text) is not None
                    and references_verified(text, scope)
                )
            ):
                unverified.append(f"{shown(path)} in {rel}")
                continue
            where = (
                f"a key under {shown(path) or 'the document root'}"
                if is_key
                else shown(path)
            )
            for value, other in sensitive.items():
                # A key the value is sensitive at (`password` for `word`) is structure.
                if (
                    other.is_int
                    or value not in text
                    or (is_key and text in other.sensitive_keys)
                ):
                    continue
                if other.from_secret:
                    # An SSID inside a name is common; warn without the value.
                    _LOGGER.warning(
                        "store_yaml: the value of !secret %s (sensitive at %s) "
                        "appears inside the value at %s in %s; substring "
                        "occurrences are not redacted",
                        other.secret_name,
                        other.config_path,
                        where,
                        rel,
                    )
                else:
                    embedded.append(f"{other.config_path} (inside {where} in {rel})")
    for hits, why in (
        (
            key_hits,
            "are also used as mapping keys, which the redaction swap would corrupt",
        ),
        (
            embedded,
            "appear embedded inside larger values, which redaction does not replace",
        ),
        (
            unverified,
            (
                "cannot be confirmed redacted: they sit where credentials go but are "
                "not in the validated config (overridden by a later package, or a git "
                "package password used before validation)"
            ),
        ),
    ):
        if hits:
            raise EsphomeError(
                f"store_yaml: sensitive value(s) {why}: {', '.join(hits)}. {_FIX}"
            )


def _collect_sensitive_values() -> tuple[
    dict[str, _SensitiveValue], set[tuple[str, ...]]
]:
    """Map each cv.sensitive value to its `!secret` name (inline values get one from
    their path) and collect every sensitive path, empty values included."""
    used = yaml_util.registered_secret_names()
    result: dict[str, _SensitiveValue] = {}
    paths: set[tuple[str, ...]] = set()
    for path, node, is_key in _iter_nodes(CORE.config):
        if is_key or not isinstance(node, yaml_util.Sensitive):
            continue
        paths.add(path)
        if isinstance(node, str) and not node:
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
        entry.is_int = entry.is_int and isinstance(node, int)
        if path:
            entry.sensitive_keys.add(path[-1])
    return result, paths


def _symlink_descriptions(discovered: yaml_util.DiscoveredYamlFiles) -> list[str]:
    """Each directory symlink the config went through, as `link -> target`."""
    root = yaml_util.named_path(CORE.config_path).parent
    return [
        f"{link.relative_to(root, walk_up=True).as_posix()} -> "
        f"{Path(os.path.relpath(target, link.parent)).as_posix()}"
        for link, target in discovered.links.items()
    ]


def _uncaptured_note(
    unresolved: list[str], remote_packages: list[str], symlinks: list[str] = ()
) -> tuple[str, bytes] | None:
    """A comment-only entry listing uncaptured content, or None if there is none."""
    sections = (
        (
            "These !include paths use substitutions; restore the files manually:",
            unresolved,
        ),
        (
            "These packages come from remote sources; re-fetch them to complete this config:",
            remote_packages,
        ),
        ("These directories were symlinks; recreate them as link -> target:", symlinks),
    )
    body = "".join(
        f"# {title}\n" + "".join(f"#   {item}\n" for item in items)
        for title, items in sections
        if items
    )
    if not body:
        return None
    header = "# store_yaml: the following content could not be captured.\n"
    return UNCAPTURED_NOTE_PATH, (header + body).encode("utf-8")


def _remote_package_descriptions() -> list[str]:
    """Each remote package source as its secret name or masked URL, plus ref."""
    return [
        (
            f"!secret {name}"
            if (name := yaml_util.is_secret(source.url)) is not None
            else URL_USERINFO_RE.sub("://***@", source.url)
        )
        + (f"@{source.ref}" if source.ref else "")
        for source in packages.get_remote_package_sources()
    ]


def _build_secrets_skeleton(keys: set[str]) -> bytes:
    # Plain PyYAML quotes a key such as "true" and does no secret substitution.
    body = yaml.safe_dump(dict.fromkeys(sorted(keys), ""), sort_keys=False)
    return (SECRETS_SKELETON_HEADER + (body if keys else "")).encode("utf-8")


def _prepare_tree(
    node: object,
    source: Path,
    fold_secrets: bool,
    includes: list[_Include],
    sensitive: dict[str, _SensitiveValue],
    path: tuple[str, ...] = (),
) -> object:
    """Fold container secrets to their registered text (a plain str would skip the
    secret check), swap integer credentials at their own keys, point each `!include`
    at its target's captured name and collect it."""
    if (
        fold_secrets
        and isinstance(node, (dict, list))
        and yaml_util.is_secret(node) is not None
    ):
        return yaml_util.make_data_base(str(node))
    if isinstance(node, dict):
        for key, value in node.items():
            node[key] = _prepare_tree(
                value, source, fold_secrets, includes, sensitive, (*path, str(key))
            )
    elif isinstance(node, list):
        for i, item in enumerate(node):
            node[i] = _prepare_tree(
                item, source, fold_secrets, includes, sensitive, path
            )
    elif isinstance(node, int) and not isinstance(node, bool):
        info = sensitive.get(str(node))
        if (
            info is not None
            and info.is_int
            and path
            and path[-1] in info.sensitive_keys
        ):
            return yaml_util.make_data_base(_INT_MARK + info.secret_name)
    elif isinstance(node, yaml_util.IncludeFile):
        vars = node.vars if isinstance(node.vars, dict) else {}
        # A vars mapping loaded through `!secret` keeps every value secret.
        secret_vars = yaml_util.is_secret(vars) is not None
        includes.append(
            (
                source,
                node,
                {str(k): vars if secret_vars else v for k, v in vars.items()},
                path,
            )
        )
        if node.vars:
            node.vars = _prepare_tree(
                node.vars, source, fold_secrets, includes, sensitive, path
            )
        if not node.has_unresolved_file():
            # Where the file lands, through directory symlinks and `!include_dir_*` moves.
            origin = yaml_util.named_path(node.parent_file)
            target = yaml_util.named_path(origin.parent / node.file)
            node = node.with_file(
                Path(os.path.relpath(target, source.parent)).as_posix()
            )
            node.parent_file = source
    return node


def _generate_redacted_files(
    entries: list[tuple[str, Path]], secret_rels: set[str], remote_packages: list[str]
) -> list[tuple[str, bytes]]:
    """Re-dump captured files with sensitive values as `!secret`, secrets files as a
    skeleton; comments and formatting are lost."""
    sensitive, sensitive_paths = _collect_sensitive_values()

    captured = {rel: path for rel, path in entries if rel not in secret_rels}
    loaded = {}
    for rel, path in captured.items():
        # A document that is one `!include` keeps its vars and condition.
        with path.open(encoding="utf-8") as handle:
            loaded[rel] = yaml_util.parse_yaml(
                path, handle, resolve_document_include=False
            )
    fold_secrets = yaml_util.has_container_secrets()
    includes: list[_Include] = []
    trees = {
        rel: _prepare_tree(
            tree, yaml_util.named_path(captured[rel]), fold_secrets, includes, sensitive
        )
        for rel, tree in loaded.items()
    }
    scopes, prefixes = _include_scopes(includes, trees, captured)

    # An integer credential is swapped by _prepare_tree, not by the dumper, so a
    # short number elsewhere (`accuracy_decimals: 0`) is left alone.
    registered = {
        (_INT_MARK + info.secret_name if info.is_int else value): info.secret_name
        for value, info in sensitive.items()
    }
    with yaml_util.secret_values_registered(registered) as skeleton_keys:
        _check_sensitive_usage(sensitive, sensitive_paths, trees, scopes, prefixes)
        texts = {rel: yaml_util.dump(tree) for rel, tree in trees.items()}

    # An inline value whose name was never emitted was not found as a whole scalar.
    leaked = [
        info.config_path
        for info in sensitive.values()
        if not info.from_secret and info.secret_name not in skeleton_keys
    ]
    if leaked:
        raise EsphomeError(
            f"store_yaml: could not redact the sensitive value(s) of {', '.join(leaked)}; "
            "not found in any captured file (composed via substitutions, set with -s, "
            f"or defined in a remote package: {', '.join(remote_packages) or 'none'}). "
            + _FIX
        )

    skeleton = _build_secrets_skeleton(skeleton_keys)
    result = [
        (rel, skeleton if rel in secret_rels else texts[rel].encode("utf-8"))
        for rel, _ in entries
    ]
    if skeleton_keys and yaml_util.SECRET_YAML not in secret_rels:
        # `!secret` looks for secrets.yaml beside the config; ship a skeleton there.
        result.append((yaml_util.SECRET_YAML, skeleton))
    return result


def _pack_envelope(files: list[tuple[str, bytes]]) -> bytes:
    """EHY1 | u32 count | {u16 path_len, path, u32 content_len, content}..., little-endian."""
    paths = [path for path, _ in files]
    if len(set(paths)) != len(paths):
        # unpack_envelope would silently keep only the last duplicate.
        raise EsphomeError(f"store_yaml: duplicate envelope path in {paths}")
    return (
        ENVELOPE_MAGIC
        + struct.pack("<I", len(files))
        + b"".join(
            struct.pack("<H", len(encoded := path.encode("utf-8")))
            + encoded
            + struct.pack("<I", len(content))
            + content
            for path, content in files
        )
    )


def unpack_envelope(blob: bytes) -> dict[str, bytes]:
    """Reference decoder; rejects absolute paths, allows `..`, so disk writers confine paths."""
    if blob[:4] != ENVELOPE_MAGIC:
        raise EsphomeError("envelope must start with EHY1 magic")
    files: dict[str, bytes] = {}
    try:
        (count,) = struct.unpack_from("<I", blob, 4)
        pos = 8
        for _ in range(count):
            (path_len,) = struct.unpack_from("<H", blob, pos)
            path = blob[pos + 2 : pos + 2 + path_len].decode("utf-8")
            (size,) = struct.unpack_from("<I", blob, pos + 2 + path_len)
            pos += 6 + path_len
            if path in files or path.startswith(("/", "\\")) or path[1:2] == ":":
                raise EsphomeError(f"envelope path rejected: {path}")
            files[path], pos = blob[pos : pos + size], pos + size
    except (struct.error, UnicodeDecodeError) as err:
        raise EsphomeError(f"corrupt envelope: {err}") from err
    if pos != len(blob):
        raise EsphomeError("corrupt envelope: size mismatch")
    return files


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_STORE_YAML")

    # A fresh parse captures only user files and refills the secret registry.
    discovered = yaml_util.discover_user_yaml_files(CORE.config_path)
    entries, secret_rels = _gather_files(discovered)
    remote_packages = _remote_package_descriptions()
    if config[CONF_INCLUDE_SECRETS]:
        files = _read_files_verbatim(entries)
    else:
        files = _generate_redacted_files(entries, secret_rels, remote_packages)
    if remote_packages:
        _LOGGER.warning(
            "store_yaml: remote packages are not embedded, only recorded: %s",
            ", ".join(remote_packages),
        )
    note = _uncaptured_note(
        discovered.unresolved, remote_packages, _symlink_descriptions(discovered)
    )
    if note is not None:
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
