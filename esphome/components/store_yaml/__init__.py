from __future__ import annotations

from collections.abc import Generator
from dataclasses import dataclass
import logging
from pathlib import Path
import struct

from esphome import yaml_util
import esphome.codegen as cg
from esphome.components import packages
from esphome.components.api import CONF_ENCRYPTION
from esphome.config_helpers import Extend, Remove
import esphome.config_validation as cv
from esphome.const import CONF_API, CONF_ID, CONF_KEY, CONF_RAW_DATA_ID
from esphome.core import CORE, EsphomeError, HexInt, Lambda
import esphome.final_validate as fv
from esphome.helpers import ensure_unique_string
from esphome.types import ConfigType

try:
    from compression import zstd  # Python 3.14+ stdlib
except ImportError:
    from backports import zstd  # pinned in requirements.txt for Python < 3.14

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@bdraco"]
DEPENDENCIES = ["api"]

CONF_INCLUDE_SECRETS = "include_secrets"
CONF_ALLOW_UNENCRYPTED = "allow_unencrypted"

store_yaml_ns = cg.esphome_ns.namespace("store_yaml")
StoreYamlComponent = store_yaml_ns.class_("StoreYamlComponent", cg.Component)

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

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(StoreYamlComponent),
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        cv.Optional(CONF_INCLUDE_SECRETS, default=False): cv.boolean,
        cv.Optional(CONF_ALLOW_UNENCRYPTED, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


def _final_validate(config: ConfigType) -> ConfigType:
    """Require API encryption: an unauthenticated client could otherwise pull
    the embedded YAML (which may include Wi-Fi credentials or opted-in
    secrets). The escape hatch ``allow_unencrypted: true`` exists for
    isolated lab setups where the user has accepted the trade-off."""
    full = fv.full_config.get()
    api_conf = full.get(CONF_API, {})
    # Explicitly require a configured key: a keyless provisionable
    # `api: encryption:` accepts the well-known all-zeros PSK until
    # provisioned, so anyone could provision it and pull the YAML.
    encryption = api_conf.get(CONF_ENCRYPTION) or {}
    if CONF_KEY in encryption:
        return config
    if config.get(CONF_ALLOW_UNENCRYPTED):
        if config.get(CONF_INCLUDE_SECRETS):
            _LOGGER.warning(
                "store_yaml is enabled without API encryption and with "
                "include_secrets; any client that can reach the device on the "
                "network can pull the embedded YAML including the verbatim "
                "contents of secrets.yaml."
            )
        else:
            _LOGGER.warning(
                "store_yaml is enabled without API encryption; any client that "
                "can reach the device on the network can pull the embedded YAML."
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

    Returns (relative_path, source_path) pairs plus the subset of relative
    paths that are secrets files (matched upstream on the *un-resolved*
    basename, so a `secrets.yaml` symlinked to a differently-named target is
    still flagged).
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

    # Resolved (not just absolute) because discovery returns resolved paths
    # and relative_to needs both sides on the same footing when symlinks are
    # in play; CORE.config_dir is only absolute.
    root = CORE.config_path.resolve().parent

    entries: list[tuple[str, Path]] = []
    secret_rels: set[str] = set()
    for path in discovered.files:
        # Files outside the project root (e.g. ../common.yaml or a secrets file
        # in $HOME) keep their ".." components so the include graph is preserved
        # and files from different directories with the same basename don't
        # collide.
        try:
            rel_str = path.relative_to(root, walk_up=True).as_posix()
        except ValueError as err:
            # Different anchors (a Windows file on another drive) cannot be
            # expressed relative to the config root.
            raise EsphomeError(
                f"store_yaml: cannot place {path} in the recovery envelope; "
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
            # A silently partial recovery blob defeats the feature; fail the
            # build instead of embedding an incomplete file set.
            raise EsphomeError(
                f"store_yaml: cannot read tracked YAML file {path}: {err}"
            ) from err
    return files


def _iter_nodes(
    node: object, path: tuple[str, ...] = ()
) -> Generator[tuple[tuple[str, ...], object, bool]]:
    """Yield (config_path, value, is_key) for every mapping key and scalar in
    a config tree. Keys are yielded at the path of their mapping.

    Wrapper types the dumper renders as text are unwrapped so their payloads
    are scanned too: `!lambda` bodies, `!extend`/`!remove` ids, and `!include`
    file paths plus `vars:` values.
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
        if node.vars:
            yield from _iter_nodes(node.vars, path)
    elif isinstance(node, (str, int, float)) and not isinstance(node, bool):
        yield path, node, False


@dataclass
class _SensitiveValue:
    secret_name: str
    config_path: str  # dotted path, for warnings (never log the value itself)
    # Last path segments the value is sensitive at (e.g. {"password"}), used to
    # tell the value's own occurrences apart from unrelated collisions.
    sensitive_keys: set[str]


def _check_sensitive_usage(
    sensitive: dict[str, _SensitiveValue], trees: dict[str, object]
) -> None:
    """One pass over the parse trees that will be dumped, checking every place
    a sensitive value shows up beyond its own whole-scalar occurrences.

    - As a mapping key: fail the build. The dumper's value-keyed swap would
      rewrite the key and corrupt the recovered structure.
    - Strictly inside a larger scalar (a lambda body, a URL like
      http://user:pw@host): the whole-scalar swap cannot redact it, so it
      would ship verbatim. Inline sensitive values fail the build (the
      move-into-!secret remedy applies); values already sourced from a real
      `!secret` only warn, since overlaps like an SSID inside an entity name
      are common and the value stays in the user's secrets.yaml either way.
      Scanning tree scalars (not serialized text) means key names, tags, and
      generated `!secret` references can never false-positive.
    - As a whole scalar at an unrelated location: warn only. The swap rewrites
      it to the `!secret` reference, which stays semantically identical until
      the user fills in a different value during recovery. Occurrences under
      the value's own sensitive key and swapped `substitutions:` definitions
      (which keep `${...}` references working) are expected and stay silent.
    """
    if not sensitive:
        return
    key_hits: list[str] = []
    embedded: list[str] = []
    for rel, tree in trees.items():
        for path, node, is_key in _iter_nodes(tree):
            text = str(node)
            info = sensitive.get(text)
            if is_key:
                if info is not None:
                    key_hits.append(
                        f"{info.config_path} (as the mapping key at "
                        f"{'.'.join((*path, text))} in {rel})"
                    )
                continue
            if info is not None:
                if path and (
                    path[0] == "substitutions" or path[-1] in info.sensitive_keys
                ):
                    continue
                _LOGGER.warning(
                    "store_yaml: the sensitive value at %s also matches the "
                    "scalar at %s in %s; the recovered config will reference "
                    "!secret %s there too",
                    info.config_path,
                    ".".join(path),
                    rel,
                    info.secret_name,
                )
                continue
            for value, other in sensitive.items():
                if value not in text:
                    continue
                if yaml_util.is_secret(value) is not None:
                    # The value lives in a real secrets.yaml, so the
                    # move-into-!secret remedy does not apply; overlaps like
                    # an SSID inside an entity name are common and benign.
                    # Warn naming the location, never the value.
                    _LOGGER.warning(
                        "store_yaml: the value of !secret %s (sensitive at %s) "
                        "appears inside the value at %s in %s; substring "
                        "occurrences are not redacted",
                        other.secret_name,
                        other.config_path,
                        ".".join(path),
                        rel,
                    )
                else:
                    embedded.append(
                        f"{other.config_path} (inside {'.'.join(path)} in {rel})"
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


def _collect_sensitive_values() -> dict[str, _SensitiveValue]:
    """Map each cv.sensitive value in the validated config to the `!secret`
    name it should be recovered as.

    Values that already come from `!secret` keep their existing name; inline
    values get a name generated from their config path, avoiding names already
    taken by real secrets.
    """
    used = yaml_util.registered_secret_names()
    result: dict[str, _SensitiveValue] = {}
    for path, node, is_key in _iter_nodes(CORE.config):
        if is_key or not isinstance(node, yaml_util.SensitiveStr) or not node:
            continue
        value = str(node)
        entry = result.get(value)
        if entry is None:
            name = yaml_util.is_secret(value)
            if name is None:
                name = ensure_unique_string("_".join(path) or "secret", used)
            used.add(name)
            entry = result[value] = _SensitiveValue(name, ".".join(path), set())
        if path:
            entry.sensitive_keys.add(path[-1])
    return result


def _uncaptured_note(
    unresolved: list[str], remote_packages: list[str]
) -> tuple[str, bytes] | None:
    """Comment-only YAML entry listing content that could not be captured, so
    a recovered config never silently appears complete. Emitted for both the
    redacted and verbatim paths; user files are never modified to carry it.
    Returns None when there is nothing to record."""
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


def _remote_package_descriptions() -> list[str]:
    """Describe every remote source packages were fetched from.

    Remote packages are downloaded while the config is processed; the packages
    component records each source, and this formats that record. Their files
    cannot be embedded, but the entry file still records the package config, so
    the config is re-fetchable; this only makes the gap visible instead of
    silent.
    """
    return [
        f"{source.url}@{source.ref}" if source.ref else source.url
        for source in packages.get_remote_package_sources()
    ]


def _build_secrets_skeleton(keys: set[str]) -> bytes:
    parts = [SECRETS_SKELETON_HEADER]
    parts.extend(f'{key}: ""\n' for key in sorted(keys))
    return "".join(parts).encode("utf-8")


def _generate_redacted_files(
    entries: list[tuple[str, Path]], secret_rels: set[str]
) -> list[tuple[str, bytes]]:
    """Re-generate each captured file from its parse tree with cv.sensitive
    values emitted as `!secret <name>` references, and replace secrets files
    with a fill-in skeleton — the recovered config is flashable once the user
    restores their secrets.yaml values.

    The swap happens inside the YAML dumper (`represent_stringify` consults
    the registered secret values), not by mutating text afterwards. Nested
    `!include` references round-trip via the dumper's IncludeFile support;
    comments and formatting of the originals are not preserved.
    """
    sensitive = _collect_sensitive_values()

    trees = {
        rel: yaml_util.load_yaml(path, clear_secrets=False)
        for rel, path in entries
        if rel not in secret_rels
    }

    _check_sensitive_usage(sensitive, trees)

    registered = {value: info.secret_name for value, info in sensitive.items()}
    with yaml_util.secret_values_registered(registered) as skeleton_keys:
        texts = {rel: yaml_util.dump(tree) for rel, tree in trees.items()}

    # skeleton_keys now holds exactly the `!secret` names the dumper emitted.
    # A registered inline value whose name was never emitted was not found as
    # a whole scalar in any captured file, so it would ship nowhere and the
    # redaction promise cannot be verified — fail the build.
    leaked = [
        info.config_path
        for value, info in sensitive.items()
        if yaml_util.is_secret(value) is None and info.secret_name not in skeleton_keys
    ]
    if leaked:
        remote = _remote_package_descriptions()
        raise EsphomeError(
            "store_yaml: could not redact the sensitive value(s) of "
            f"{', '.join(leaked)}. The value was not found in any captured "
            "file; it may be composed via substitutions, set on the command "
            "line with -s, or defined inside a remote package"
            + (f" ({', '.join(remote)})" if remote else "")
            + ". Reference it with `!secret` in the YAML, or set "
            "`include_secrets: true` to embed secrets deliberately."
        )

    skeleton = _build_secrets_skeleton(skeleton_keys)
    result = [
        (rel, skeleton if rel in secret_rels else texts[rel].encode("utf-8"))
        for rel, _ in entries
    ]
    if skeleton_keys and yaml_util.SECRET_YAML not in secret_rels:
        # The generated files reference `!secret` keys but no captured secrets
        # file lands at the config root (none exists, or it resolves outside
        # the root, e.g. a symlink target). `!secret` resolution looks for
        # secrets.yaml beside the config, so ship a synthetic root skeleton to
        # keep the recovered config loadable.
        result.append((yaml_util.SECRET_YAML, skeleton))
    return result


def _pack_envelope(files: list[tuple[str, bytes]]) -> bytes:
    """Pack files into the EHY1 envelope.

    Layout: magic (4) | u32 file_count | repeat { u16 path_len | path_utf8 | u32 content_len | content_bytes }
    All integers are little-endian.
    """
    parts: list[bytes] = [ENVELOPE_MAGIC, struct.pack("<I", len(files))]
    seen: set[str] = set()
    for path, content in files:
        if path in seen:
            # unpack_envelope builds a dict, so a duplicate would silently
            # replace the earlier entry — fail the build instead.
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
    """Inverse of `_pack_envelope`: the reference decoder for the EHY1 envelope,
    used by tests and client-side recovery tooling.

    Absolute and drive-qualified paths are rejected: the packer never emits
    them, so their presence means a malformed or hostile envelope. Relative
    paths with ``..`` components are legitimate (the packer emits them for
    files outside the config root), so callers that write files to disk must
    still confine the resulting paths to their target directory."""
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

    # Discover the user's on-disk YAML files via a fresh re-parse — same
    # pattern bundle.py uses. Running at codegen time (rather than keeping a
    # listener installed across validation) avoids capturing framework YAML
    # that components load internally (e.g. LVGL's `hello_world.yaml`), and
    # costs nothing on validate-only runs or configs without this component.
    discovered = yaml_util.discover_user_yaml_files(CORE.config_path)
    entries, secret_rels = _gather_files(discovered)
    if config[CONF_INCLUDE_SECRETS]:
        files = _read_files_verbatim(entries)
    else:
        files = _generate_redacted_files(entries, secret_rels)
    remote_packages = _remote_package_descriptions()
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
    compressed = zstd.compress(envelope, level=ZSTD_LEVEL)

    _LOGGER.info(
        "store_yaml: embedding %d file(s) as %d bytes (%d uncompressed, %.1f%% ratio)",
        len(files),
        len(compressed),
        len(envelope),
        100.0 * len(compressed) / len(envelope),
    )

    rhs = [HexInt(b) for b in compressed]
    prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_data(prog_arr, len(compressed), len(envelope)))
