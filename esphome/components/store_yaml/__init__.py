from __future__ import annotations

import logging
from pathlib import Path
import struct
from types import ModuleType

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_RAW_DATA_ID, SECRETS_FILES
from esphome.core import CORE, EsphomeError, HexInt

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@bdraco"]
DEPENDENCIES = ["api"]

CONF_INCLUDE_SECRETS = "include_secrets"

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
    }
).extend(cv.COMPONENT_SCHEMA)


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
    sources = CORE.data.get("yaml_sources")
    if not sources:
        raise EsphomeError(
            "store_yaml could not find any tracked YAML files; the config loader "
            "did not populate CORE.data['yaml_sources']."
        )

    config_path = Path(CORE.config_path).resolve()
    root = config_path.parent

    seen: set[Path] = set()
    files: list[tuple[str, bytes]] = []
    for path in sources:
        resolved = Path(path).resolve()
        if resolved in seen:
            continue
        seen.add(resolved)

        # Either secrets.yaml or secrets.yml. Symlinks are followed by resolve()
        # above, so we re-check the original path's name too in case someone
        # symlinks `secrets.yaml` to a differently-named target.
        is_secret = resolved.name in SECRETS_FILES or Path(path).name in SECRETS_FILES
        if is_secret and not include_secrets:
            content = REDACTED_PLACEHOLDER
        else:
            try:
                content = resolved.read_bytes()
            except OSError as err:
                _LOGGER.warning(
                    "store_yaml: skipping unreadable %s (%s)", resolved, err
                )
                continue

        try:
            rel = resolved.relative_to(root)
            rel_str = rel.as_posix()
        except ValueError:
            # Outside the project root (e.g. secrets.yaml in $HOME); store basename only.
            rel_str = resolved.name

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


async def to_code(config):
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
