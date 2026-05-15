from __future__ import annotations

import logging
import os
from pathlib import Path
import struct
from types import ModuleType

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_API, CONF_ID, CONF_RAW_DATA_ID
from esphome.core import CORE, EsphomeError, HexInt
import esphome.final_validate as fv
from esphome.types import ConfigType

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
# Replacement content when secrets are not included.
REDACTED_PLACEHOLDER = b"# redacted\n"

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
    secrets). The escape hatch ``allow_unencrypted_api: true`` exists for
    isolated lab setups where the user has accepted the trade-off."""
    full = fv.full_config.get()
    api_conf = full.get(CONF_API, {})
    if api_conf.get("encryption"):
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


def _import_zstd() -> ModuleType:
    try:
        from compression import zstd  # noqa: PLC0415 — Python 3.14+ stdlib
    except ImportError:
        try:
            from backports import zstd  # noqa: PLC0415
        except ImportError as err:
            raise EsphomeError(
                "store_yaml requires zstd compression. Install backports.zstd for "
                "Python < 3.14 or upgrade to Python 3.14+."
            ) from err
    return zstd


def _gather_files(include_secrets: bool) -> list[tuple[str, bytes]]:
    """Read each YAML file the config loader touched, return (relative_path, content) pairs."""
    discovered = CORE.data.get("yaml_sources")
    if not discovered or not discovered.files:
        raise EsphomeError(
            "store_yaml could not find any tracked YAML files; the config loader "
            "did not populate CORE.data['yaml_sources']."
        )

    config_path = Path(CORE.config_path).resolve()
    root = config_path.parent
    secret_paths = discovered.secrets

    files: list[tuple[str, bytes]] = []
    for path in discovered.files:
        # `secret_paths` was collected from the *un-resolved* basename, so a
        # `secrets.yaml` symlinked to a differently-named target is still
        # treated as secrets here.
        if path in secret_paths and not include_secrets:
            content = REDACTED_PLACEHOLDER
        else:
            try:
                content = path.read_bytes()
            except OSError as err:
                _LOGGER.warning("store_yaml: skipping unreadable %s (%s)", path, err)
                continue

        try:
            rel_str = path.relative_to(root).as_posix()
        except ValueError:
            # Outside the project root (e.g. ../common.yaml or a secrets file in
            # $HOME). Use a relative path with ".." components instead of just
            # the basename so the include graph is preserved and files from
            # different directories with the same basename don't collide.
            rel_str = os.path.relpath(path, root).replace(os.sep, "/")

        files.append((rel_str, content))

    return files


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


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_STORE_YAML")

    zstd = _import_zstd()

    files = _gather_files(config[CONF_INCLUDE_SECRETS])
    envelope = _pack_envelope(files)
    compressed = zstd.compress(envelope, level=ZSTD_LEVEL)

    _LOGGER.info(
        "store_yaml: embedding %d file(s) as %d bytes (%d uncompressed, %.1f%% ratio)",
        len(files),
        len(compressed),
        len(envelope),
        100.0 * len(compressed) / max(1, len(envelope)),
    )

    rhs = [HexInt(b) for b in compressed]
    prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_data(prog_arr, len(compressed), len(envelope)))
