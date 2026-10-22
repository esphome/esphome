from __future__ import annotations

from collections.abc import Generator
from dataclasses import dataclass
import logging
import os
from pathlib import Path
import re
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
    CONF_ID,
    CONF_PACKAGES,
    CONF_SSID,
    CONF_SUBSTITUTIONS,
)
from esphome.core import CORE, EsphomeError, Lambda
from esphome.expression import SUBSTITUTION_VARIABLE_PROG
import esphome.final_validate as fv
from esphome.git import URL_USERINFO_RE
from esphome.helpers import ensure_unique_string, list_starts_with, zstd_module
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
# An `!include`, its vars before any folding, and the config path of its site.
_Include = tuple[yaml_util.IncludeFile, dict[str, object], tuple[str, ...]]
# Stands in for an integer credential at its keys and definitions.
_INT_MARK = "\x00"
# A removed branch is never validated, so its credentials are known only by name.
_CREDENTIAL_WORDS = ("password", "key", "secret", "token", "psk", "tlv", "pairing_code")
# Every key validated with cv.sensitive (a test keeps this complete) and the
# packet_transport `encryption: <key>` shorthand: credentials wherever they appear,
# also when a merge or override drops them before validation. SSIDs are broadcast,
# and a bare `key` also names keypad keys; both are checked where validation kept them.
_SENSITIVE_KEYS = frozenset(
    {"password", "new_password", "private_key", "peer_preshared_key"}
    | {"client_certificate_key", "network_key", "old_key", "pskc", "tlv"}
    | {"static_pairing_code", "encryption"}
)
# Keys under these name declared arguments, not values (`variables: {password: string}`).
_DECLARATIONS = frozenset({"variables", "parameters"})
# A quoted string or a number: a lambda without one cannot spell a credential.
_LITERAL_RE = re.compile(r"""["']|(?<![\w.])\d""")

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
    elif isinstance(node, Lambda):
        yield path, node, False
    elif isinstance(node, (Extend, Remove)):
        yield path, node.value, False
    elif isinstance(node, yaml_util.IncludeFile):
        yield path, str(node.file), False
        if isinstance(node.condition, str):
            yield path, node.condition, False
        if node.vars:
            yield from _iter_nodes(node.vars, path)
    elif isinstance(node, (str, int, float)) and not isinstance(node, bool):
        yield path, node, False


def _str_keys(value: object) -> dict[str, object]:
    """`value` as a mapping with string keys, or empty."""
    return {str(k): v for k, v in value.items()} if isinstance(value, dict) else {}


def _ends_with(full: tuple[str, ...], part: tuple[str, ...]) -> bool:
    """`part` ends `full`, skipping segments validation inserted (`wifi.password`
    to `wifi.networks.password`)."""
    if full[-1] != part[-1]:
        return False
    rest = iter(reversed(full[:-1]))
    return all(seg in rest for seg in reversed(part[:-1]))


def _merged(path: tuple[str, ...]) -> set[tuple[str, ...]]:
    """`path` as validation sees it: every `packages[.<name>]` prefix merges away."""
    if path and path[0] == CONF_PACKAGES:
        return _merged(path[1:]) | _merged(path[2:])
    return {path}


def _defines_substitution(path: tuple[str, ...]) -> bool:
    """`path` is a `substitutions:` entry, also inside an inline package."""
    return any(len(p) > 1 and p[0] == CONF_SUBSTITUTIONS for p in _merged(path))


@dataclass
class _SensitiveValue:
    secret_name: str
    config_path: str  # dotted path, for messages (never the value itself)
    from_secret: bool  # loaded through a real `!secret`
    # Keys the value sits at, to tell its own occurrences from collisions.
    sensitive_keys: set[str]
    # Integers skip the substring scan; short PINs would match unrelated text.
    is_int: bool = True


def _include_scopes(
    includes: dict[str, list[_Include]],
    trees: dict[str, object],
    sources: dict[str, Path],
) -> tuple[dict[str, _Scope], dict[str, list[tuple[str, ...]]]]:
    """Per file: every value a substitution name may take (per include: its vars,
    else the file's `defaults:`, else the caller's chain, else substitutions) and
    the config paths its content lands under."""
    substitutions: _Scope = {}
    for tree in trees.values():
        for path, node, is_key in _iter_nodes(tree):
            if not is_key and _defines_substitution(path):
                substitutions.setdefault(path[-1], []).append(node)
    defaults = {
        rel: _str_keys(_str_keys(tree).get(CONF_DEFAULTS))
        for rel, tree in trees.items()
    }
    rels_by_path: dict[Path, list[str]] = {}
    for rel, path in sources.items():
        rels_by_path.setdefault(path.resolve(), []).append(rel)
    edges: dict[str, list[tuple[str, dict[str, object], tuple[str, ...]]]] = {}
    for src, items in includes.items():
        for include, vars, site in items:
            if include.has_unresolved_file() or not yaml_util.include_enabled(include):
                continue
            target = (include.parent_file.parent / include.file).resolve()
            edges.setdefault(src, []).extend(
                (rel, vars, site) for rel in rels_by_path.get(target, ())
            )
    # Each way a file is reached: its vars chain, built as the loader does, and
    # the include-site prefix; a caller's chain flows into nested includes.
    targeted = {rel for targets in edges.values() for rel, _, _ in targets}
    invocations: dict[str, list[tuple[dict[str, object], tuple[str, ...]]]] = {}
    pending = [(rel, defaults[rel], (), ()) for rel in trees if rel not in targeted]
    while pending:
        rel, chain, prefix, lineage = pending.pop()
        if (chain, prefix) in invocations.setdefault(rel, []):
            continue
        invocations[rel].append((chain, prefix))
        pending.extend(
            (dst, {**chain, **defaults[dst], **vars}, prefix + site, (*lineage, rel))
            for dst, vars, site in edges.get(rel, ())
            if dst not in lineage  # an include cycle never ends otherwise
        )
    scopes: dict[str, _Scope] = {}
    prefixes: dict[str, list[tuple[str, ...]]] = {}
    for rel in trees:
        chains = invocations.get(rel) or [(defaults[rel], ())]
        scope: _Scope = {}
        for chain, _ in chains:
            for name in {*substitutions, *chain}:
                scope.setdefault(name, []).extend(
                    [chain[name]] if name in chain else substitutions[name]
                )
        scopes[rel] = scope
        prefixes[rel] = [prefix for _, prefix in chains]
    return scopes, prefixes


def _check_sensitive_usage(
    sensitive: dict[str, _SensitiveValue],
    sensitive_paths: set[tuple[str, ...]],
    trees: dict[str, object],
    scopes: dict[str, _Scope],
    prefixes: dict[str, list[tuple[str, ...]]],
    removed: set[tuple[str, ...]],
) -> None:
    """A sensitive value is a whole scalar (swapped) or fails, except a secret SSID
    inside a name (warned); unverifiable credential locations and URL userinfo fail."""
    credential_keys = {sp[-1] for sp in sensitive_paths if sp}

    def verified(text: str) -> bool:
        return not text or text in sensitive or yaml_util.is_secret(text) is not None

    def references_verified(
        text: str, scope: _Scope, seen: frozenset = frozenset()
    ) -> bool:
        """Every reference names definitions that are verified or alias verified ones."""
        names = [m.strip("{}") for m in SUBSTITUTION_VARIABLE_PROG.findall(text)]
        return bool(names) and all(
            name in scope
            and name not in seen
            and all(
                verified(str(v))
                or SUBSTITUTION_VARIABLE_PROG.fullmatch(str(v)) is not None
                and references_verified(str(v), scope, seen | {name})
                for v in scope[name]
            )
            for name in names
        )

    def credential_path(path: tuple[str, ...]) -> bool:
        """Whether `path` lands where the validated config holds a credential."""
        if not path:
            return False
        merged = _merged(path)
        if path[-1] in _SENSITIVE_KEYS and not _DECLARATIONS.intersection(path[-2:-1]):
            return True
        if any(p and any(list_starts_with(p, r) for r in removed) for p in merged):
            return path[-1] in credential_keys or any(
                word in path[-1] for word in _CREDENTIAL_WORDS
            )
        return path[-1] in credential_keys and any(
            p and any(_ends_with(sp, p) for sp in sensitive_paths) for p in merged
        )

    def shown(path: tuple[str, ...]) -> str:
        """`path` for messages, credential segments masked."""
        return ".".join(
            f"<!secret {sensitive[seg].secret_name}>" if seg in sensitive else seg
            for seg in path
        )

    failures: dict[str, list[str]] = {
        "appear embedded inside larger values or as mapping keys, which redaction "
        "does not replace": [],
        "cannot be confirmed redacted: they sit at a credential key or location but "
        "are not a known secret (for example a literal password, or one a merge, "
        "override or `!remove` dropped before validation)": [],
    }
    embedded, unverified = failures.values()
    for rel, tree in trees.items():
        scope = scopes[rel]
        for path, node, is_key in _iter_nodes(tree):
            text = str(node)
            where = (
                f"a mapping key under {shown(path) or 'the document root'}"
                if is_key
                else shown(path)
            )
            if verified(text):
                # A whole scalar is swapped for its `!secret`; a key never is.
                if is_key and (info := sensitive.get(text)) is not None:
                    embedded.append(f"{info.config_path} (as {where} in {rel})")
                continue
            if not is_key and any(
                SUBSTITUTION_VARIABLE_PROG.sub("", userinfo) not in ("", ":")
                or not references_verified(userinfo, scope)
                for userinfo in URL_USERINFO_RE.findall(text)
            ):
                unverified.append(f"{where} in {rel} (a URL with credentials)")
            elif (
                not is_key
                and any(credential_path(prefix + path) for prefix in prefixes[rel])
                and not (
                    _LITERAL_RE.search(text) is None
                    if isinstance(node, Lambda)
                    else SUBSTITUTION_VARIABLE_PROG.fullmatch(text) is not None
                    and references_verified(text, scope)
                )
            ):
                unverified.append(f"{where} in {rel}")
                continue
            for value, other in sensitive.items():
                # A key the value is sensitive at (`password` for `word`) is structure.
                if (
                    other.is_int
                    or value not in text
                    or (is_key and text in other.sensitive_keys)
                ):
                    continue
                if other.from_secret and other.sensitive_keys <= {CONF_SSID}:
                    # A broadcast SSID inside a name is common; warn without the value.
                    _LOGGER.warning(
                        "store_yaml: !secret %s is not redacted inside %s in %s",
                        other.secret_name,
                        where,
                        rel,
                    )
                else:
                    embedded.append(f"{other.config_path} (inside {where} in {rel})")
    for why, hits in failures.items():
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
        if isinstance(node, Lambda):
            if _LITERAL_RE.search(node.value) is not None:
                raise EsphomeError(
                    f"store_yaml: the credential at {'.'.join(path)} is set by a "
                    f"lambda holding a literal, which cannot be redacted. {_FIX}"
                )
            continue
        if isinstance(node, str) and not node:
            continue
        # Decimal for ints, to match the raw tree's parsed scalars, not HexInt's hex.
        value = str(int(node)) if isinstance(node, int) else str(node)
        if (entry := result.get(value)) is None:
            from_secret = (name := yaml_util.is_secret(value)) is not None
            if not from_secret:
                name = ensure_unique_string("_".join(path) or "secret", used)
                used.add(name)
            entry = result[value] = _SensitiveValue(
                name, ".".join(path), from_secret, set()
            )
        entry.is_int = entry.is_int and isinstance(node, int)
        entry.sensitive_keys.update(path[-1:])
    return result, paths


def _symlink_descriptions(discovered: yaml_util.DiscoveredYamlFiles) -> list[str]:
    """Each directory symlink the config went through, as `link -> target`."""
    root = Path(CORE.config_path).absolute().parent
    return [
        f"{link.relative_to(root, walk_up=True).as_posix()} -> "
        f"{Path(os.path.relpath(target, link.parent.resolve())).as_posix()}"
        for link, target in discovered.links.items()
    ]


def _uncaptured_note(
    unresolved: list[str], remote_packages: list[str], symlinks: list[str]
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

    def describe(text: str) -> str:
        if (name := yaml_util.is_secret(text)) is not None:
            return f"!secret {name}"
        # A secret substituted into the URL or ref is shown by name, longest first so
        # a secret containing another is replaced whole.
        secrets = yaml_util.registered_secret_values().items()
        for value, name in sorted(secrets, key=lambda item: -len(item[0])):
            if value and value in text:
                text = text.replace(value, f"<!secret {name}>")
        return URL_USERINFO_RE.sub("://***@", text)

    return [
        describe(source.url) + (f"@{describe(source.ref)}" if source.ref else "")
        for source in packages.get_remote_package_sources()
    ]


def _build_secrets_skeleton(keys: set[str]) -> bytes:
    # Plain PyYAML quotes a key such as "true" and does no secret substitution.
    body = yaml.safe_dump(dict.fromkeys(sorted(keys), ""), sort_keys=False)
    return (SECRETS_SKELETON_HEADER + (body if keys else "")).encode("utf-8")


def _generate_redacted_files(
    entries: list[tuple[str, Path]], secret_rels: set[str], remote_packages: list[str]
) -> list[tuple[str, bytes]]:
    """Re-dump captured files with sensitive values as `!secret`, secrets files as a
    skeleton; comments and formatting are lost."""
    sensitive, sensitive_paths = _collect_sensitive_values()
    includes: dict[str, list[_Include]] = {}
    removed: dict[str, set[tuple[str, ...]]] = {}

    def prepare(
        node: object,
        source: Path,
        rel: str,
        path: tuple[str, ...],
        in_vars: bool = False,
    ) -> object:
        """Fold container secrets to their registered text (a plain str would skip
        the secret check), swap integer credentials at their own keys, point each
        `!include` at its target's captured name, collect includes and removals."""
        if isinstance(node, (dict, list)) and yaml_util.is_secret(node) is not None:
            return yaml_util.make_data_base(str(node))
        if isinstance(node, dict):
            for key, value in node.items():
                node[key] = prepare(value, source, rel, (*path, str(key)), in_vars)
        elif isinstance(node, list):
            for i, item in enumerate(node):
                node[i] = prepare(item, source, rel, path, in_vars)
        elif isinstance(node, Remove):
            # `id: !remove x` drops the whole list item, not just its id.
            removed.setdefault(rel, set()).add(
                path[:-1] if path[-1:] == (CONF_ID,) else path
            )
        elif isinstance(node, (int, str)) and not isinstance(node, bool):
            # An integer credential, quoted or not, at its own keys or as a variable
            # definition a credential may reference.
            info = sensitive.get(str(node))
            at_key = info is not None and info.sensitive_keys.intersection(path[-1:])
            definition = (
                in_vars or path[:1] == (CONF_DEFAULTS,) or _defines_substitution(path)
            )
            if info and info.is_int and (at_key or definition):
                return yaml_util.make_data_base(_INT_MARK + info.secret_name)
        elif isinstance(node, yaml_util.IncludeFile):
            vars = _str_keys(node.vars)
            if yaml_util.is_secret(node.vars) is not None:
                vars = dict.fromkeys(vars, node.vars)
            includes.setdefault(rel, []).append((node, vars, path))
            if node.vars:
                node.vars = prepare(node.vars, source, rel, path, in_vars=True)
            if not node.has_unresolved_file():
                # Where the loader read it, through directory symlinks and `!include_dir_*` moves.
                target = yaml_util.named_path(Path(node.parent_file).parent / node.file)
                node = node.with_file(
                    Path(os.path.relpath(target, source.parent)).as_posix()
                )
        return node

    for rel, path in entries:
        # A secrets file becomes a key skeleton; what it includes would ship verbatim.
        if rel in secret_rels and any(
            isinstance(token, yaml.TagToken) and token.value[1].startswith("include")
            for token in yaml.scan(path.read_text(encoding="utf-8"), yaml.SafeLoader)
        ):
            raise EsphomeError(
                f"store_yaml: {rel} uses !include, which redaction cannot follow. "
                f"Inline the files into {rel}, or set `include_secrets: true`."
            )
    captured = {rel: path for rel, path in entries if rel not in secret_rels}
    trees: dict[str, object] = {}
    for rel, path in captured.items():
        # A document that is one `!include` keeps its vars and condition.
        with path.open(encoding="utf-8") as handle:
            tree = yaml_util.parse_yaml(path, handle, resolve_document_include=False)
        trees[rel] = prepare(tree, yaml_util.named_path(path), rel, ())
    scopes, prefixes = _include_scopes(includes, trees, captured)
    removed_at = {
        merged
        for rel, paths in removed.items()
        for path in paths
        for prefix in prefixes[rel]
        for merged in _merged(prefix + path)
        if merged
    }

    # An integer credential is swapped by prepare(), not by the dumper, so an
    # unrelated short number (`accuracy_decimals: 0`) is left alone.
    registered = {
        (_INT_MARK + info.secret_name if info.is_int else value): info.secret_name
        for value, info in sensitive.items()
    }
    with yaml_util.secret_values_registered(registered) as skeleton_keys:
        _check_sensitive_usage(
            sensitive, sensitive_paths, trees, scopes, prefixes, removed_at
        )
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
