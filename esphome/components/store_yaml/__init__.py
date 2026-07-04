from __future__ import annotations

from dataclasses import dataclass
import logging
import os
from pathlib import Path
import struct

from esphome import yaml_util
import esphome.codegen as cg
from esphome.components.api import CONF_ENCRYPTION
import esphome.config_validation as cv
from esphome.const import CONF_API, CONF_ID, CONF_RAW_DATA_ID
from esphome.core import CORE, EsphomeError, HexInt
import esphome.final_validate as fv
from esphome.types import ConfigType

try:
    from compression import zstd  # Python 3.14+ stdlib
except ImportError:
    from backports import zstd  # pinned in requirements.txt for Python < 3.14

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@bdraco"]
DEPENDENCIES = ["api"]

CONF_INCLUDE_SECRETS = "include_secrets"
# Avoid an `_api:` substring in the key name so the integration-test harness
# (which naively str-replaces `api:` to inject a port directive) doesn't
# clobber configs that opt into this escape hatch.
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
    if api_conf.get(CONF_ENCRYPTION):
        return config
    if config.get(CONF_ALLOW_UNENCRYPTED):
        _LOGGER.warning(
            "store_yaml is enabled without API encryption; any client that can "
            "reach the device on the network can pull the embedded YAML."
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
) -> tuple[list[tuple[str, bytes]], set[str]]:
    """Read each discovered YAML file verbatim.

    Returns (relative_path, content) pairs plus the subset of relative paths
    that are secrets files (matched upstream on the *un-resolved* basename, so
    a `secrets.yaml` symlinked to a differently-named target is still flagged).
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

    config_path = Path(CORE.config_path).resolve()
    root = config_path.parent

    files: list[tuple[str, bytes]] = []
    secret_rels: set[str] = set()
    for path in discovered.files:
        try:
            content = path.read_bytes()
        except OSError as err:
            raise EsphomeError(
                f"store_yaml: cannot read tracked YAML file {path}: {err}"
            ) from err

        try:
            rel_str = path.relative_to(root).as_posix()
        except ValueError:
            # Outside the project root (e.g. ../common.yaml or a secrets file in
            # $HOME). Use a relative path with ".." components instead of just
            # the basename so the include graph is preserved and files from
            # different directories with the same basename don't collide.
            rel_str = os.path.relpath(path, root).replace(os.sep, "/")

        if path in discovered.secrets:
            secret_rels.add(rel_str)
        files.append((rel_str, content))

    return files, secret_rels


def _iter_sensitive_values(node: object, path: tuple[str, ...] = ()):
    """Yield (config_path, value) for every cv.sensitive value in a config tree."""
    if isinstance(node, yaml_util.SensitiveStr):
        yield path, str(node)
    elif isinstance(node, dict):
        for key, value in node.items():
            yield from _iter_sensitive_values(value, (*path, str(key)))
    elif isinstance(node, (list, tuple)):
        for item in node:
            yield from _iter_sensitive_values(item, path)


@dataclass
class _SensitiveValue:
    secret_name: str
    config_path: str  # dotted path, for warnings (never log the value itself)
    from_secret: bool  # already loaded via !secret somewhere


def _collect_sensitive_values(reserved_names: set[str]) -> dict[str, _SensitiveValue]:
    """Map each cv.sensitive value in the validated config to the `!secret`
    name it should be recovered as.

    Values that already come from `!secret` keep their existing name; inline
    values get a name generated from their config path, avoiding
    `reserved_names`.
    """
    used = set(reserved_names)
    result: dict[str, _SensitiveValue] = {}
    for path, value in _iter_sensitive_values(CORE.config):
        if not value or value in result:
            continue
        if existing := yaml_util.is_secret(value):
            name = existing
        else:
            base = "_".join(path) or "secret"
            name = base
            counter = 2
            while name in used:
                name = f"{base}_{counter}"
                counter += 1
        used.add(name)
        result[value] = _SensitiveValue(name, ".".join(path), bool(existing))
    return result


def _build_secrets_skeleton(keys: set[str]) -> bytes:
    parts = [SECRETS_SKELETON_HEADER]
    parts.extend(f'{key}: ""\n' for key in sorted(keys))
    return "".join(parts).encode("utf-8")


def _generate_redacted_files(
    files: list[tuple[str, bytes]], secret_rels: set[str]
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
    sensitive = _collect_sensitive_values(yaml_util.registered_secret_names())
    inline = {
        value: info.secret_name
        for value, info in sensitive.items()
        if not info.from_secret
    }

    config_path = Path(CORE.config_path).resolve()
    root = config_path.parent
    texts: dict[str, str] = {}
    with yaml_util.secret_values_registered(inline):
        for rel, _ in files:
            if rel in secret_rels:
                continue
            tree = yaml_util.load_yaml(root / rel, clear_secrets=False)
            texts[rel] = yaml_util.dump(tree)

    skeleton_keys: set[str] = set()
    for text in texts.values():
        skeleton_keys |= yaml_util.find_secret_references(text)

    for info in sensitive.values():
        if not info.from_secret and info.secret_name not in skeleton_keys:
            _LOGGER.warning(
                "store_yaml: could not locate the sensitive value of '%s' in the "
                "source YAML (built via substitutions?); it may still be "
                "embedded verbatim",
                info.config_path,
            )

    skeleton = _build_secrets_skeleton(skeleton_keys)
    result = [
        (rel, skeleton if rel in secret_rels else texts[rel].encode("utf-8"))
        for rel, _ in files
    ]
    if skeleton_keys and not secret_rels:
        # The generated files reference `!secret` keys but the project has no
        # secrets file (all secrets were inline) — ship a synthetic one so the
        # recovered config is complete.
        result.append(("secrets.yaml", skeleton))
    return result


def _pack_envelope(files: list[tuple[str, bytes]]) -> bytes:
    """Pack files into the EHY1 envelope.

    Layout: magic (4) | u32 file_count | repeat { u16 path_len | path_utf8 | u32 content_len | content_bytes }
    All integers are little-endian.
    """
    parts: list[bytes] = [ENVELOPE_MAGIC, struct.pack("<I", len(files))]
    for path, content in files:
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
    used by tests and client-side recovery tooling."""
    if blob[:4] != ENVELOPE_MAGIC:
        raise EsphomeError("envelope must start with EHY1 magic")
    pos = 4
    files: dict[str, bytes] = {}
    try:
        (count,) = struct.unpack_from("<I", blob, pos)
        pos += 4
        for _ in range(count):
            (path_len,) = struct.unpack_from("<H", blob, pos)
            pos += 2
            if pos + path_len > len(blob):
                raise EsphomeError("truncated envelope")
            path = blob[pos : pos + path_len].decode("utf-8")
            pos += path_len
            (content_len,) = struct.unpack_from("<I", blob, pos)
            pos += 4
            if pos + content_len > len(blob):
                raise EsphomeError("truncated envelope")
            files[path] = blob[pos : pos + content_len]
            pos += content_len
    except struct.error as err:
        raise EsphomeError(f"truncated envelope: {err}") from err
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
    files, secret_rels = _gather_files(discovered)
    if not config[CONF_INCLUDE_SECRETS]:
        files = _generate_redacted_files(files, secret_rels)
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
