"""Validated-config cache for the upload/logs fast path.

compile dumps the validated config to <data_dir>/storage/<file>.validated.json;
the next upload/logs for that YAML reuses it instead of running the full
read_config pipeline. The cache is deliberately lossy: only ``!lambda``
bodies survive typed (``Lambda``); IDs, time periods, MAC/IP addresses,
paths, UUIDs and enums store the same string form the YAML dumper
produced for them. JSON additionally coerces non-str dict keys to
strings; validated configs only use string keys (every schema key
validator is ``cv.string``). mtime gates staleness.
"""

from __future__ import annotations

import json
import logging
from pathlib import Path
from typing import Any

from esphome.const import __version__ as ESPHOME_VERSION
from esphome.core import CORE, Lambda
from esphome.helpers import write_file
from esphome.storage_json import StorageJSON, ext_storage_path
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

# Bump when the on-disk shape changes; a mismatched version falls back
# to read_config. The envelope also stamps the writing esphome version:
# after an upgrade the cache holds the previous release's validation, so
# it falls back once and the re-save self-heals.
_CACHE_VERSION = 1
_LAMBDA_KEY = "__esphome_lambda__"


def compiled_config_path(config_filename: str) -> Path:
    """Path to the cached validated config alongside the storage sidecar."""
    return CORE.data_dir / "storage" / f"{config_filename}.validated.json"


def save_compiled_config(config: ConfigType) -> None:
    """Write the validated-config cache. Always-write so mtime stays fresh.

    Mode 0600 because config validation resolved !secret inline.
    Failures are non-fatal: the fast path falls back to read_config.
    """
    try:
        # The legacy YAML cache holds inline-resolved secrets and nothing
        # reads it anymore; drop it even when the write below fails. A
        # failed removal leaves resolved secrets on disk, so it warns.
        try:
            _legacy_compiled_config_path(CORE.config_filename).unlink(missing_ok=True)
        except OSError as err:
            _LOGGER.warning(
                "Could not remove the legacy validated-config cache: %s", err
            )
        rendered = json.dumps(
            {"v": _CACHE_VERSION, "esphome": ESPHOME_VERSION, "config": config},
            separators=(",", ":"),
            default=_json_default,
        )
        write_file(compiled_config_path(CORE.config_filename), rendered, private=True)
    except TypeError as err:
        # Structural, not transient: this config can never cache (e.g. a
        # non-basic dict key), so every upload/logs pays the slow path.
        _LOGGER.warning("Cannot cache the validated config: %s", err)
    except Exception as err:  # noqa: BLE001  # pylint: disable=broad-except
        _LOGGER.debug("Skipping compiled config cache write: %s", err)


def load_compiled_config(conf_path: Path) -> ConfigType | None:
    """Load the cached validated config and apply storage metadata to CORE.

    Returns None (caller falls back to read_config) when the cache is
    missing, older than the source YAML, unparseable, a different cache
    version, or the sidecar is incomplete. The loaded config carries no
    source ranges; callers must not feed it into read_config/write_cpp.
    """
    cache_path = compiled_config_path(conf_path.name)
    if not _cache_is_fresh(cache_path, conf_path):
        return None

    try:
        envelope = json.loads(
            cache_path.read_text(encoding="utf-8"), object_hook=_decode_object
        )
    except (OSError, ValueError) as err:
        _LOGGER.debug("Ignoring unreadable compiled config cache: %s", err)
        return None

    if (
        not isinstance(envelope, dict)
        or envelope.get("v") != _CACHE_VERSION
        or envelope.get("esphome") != ESPHOME_VERSION
        or not isinstance(config := envelope.get("config"), dict)
    ):
        _LOGGER.debug("Ignoring compiled config cache with a foreign envelope")
        return None

    storage = StorageJSON.load(ext_storage_path(conf_path.name))
    if storage is None:
        return None
    # apply_to_core assumes a real compile wrote the sidecar; wizard-only
    # sidecars leave both of these unset and can't drive upload/logs.
    if not storage.core_platform and not storage.target_platform:
        return None
    storage.apply_to_core()
    return config


# Remove before 2027.8: by then every maintained install has saved the
# JSON cache at least once and dropped its legacy YAML file.
def _legacy_compiled_config_path(config_filename: str) -> Path:
    """Path of the pre-JSON YAML cache; only ever removed."""
    return CORE.data_dir / "storage" / f"{config_filename}.validated.yaml"


def _cache_is_fresh(cache_path: Path, source_path: Path) -> bool:
    """True iff the cache file exists and isn't older than the source."""
    try:
        return cache_path.stat().st_mtime >= source_path.stat().st_mtime
    except OSError:
        return False


def _json_default(value: Any) -> Any:
    """Mirror ESPHomeDumper's representers: Lambda stays typed, the rest
    stringify (IDs, time periods, MAC/IP addresses, paths, UUIDs, enums).

    IncludeFile/Extend/Remove have no JSON mirror and would stringify
    wrong, but none survive validation (config.py's packages merge and
    the substitution pass consume them) so no guard is spent on them.
    """
    if isinstance(value, Lambda):
        return {_LAMBDA_KEY: value.value}
    return str(value)


def _decode_object(obj: dict[str, Any]) -> Any:
    """Revive the Lambda sentinel; every other mapping passes through."""
    if len(obj) == 1 and isinstance(value := obj.get(_LAMBDA_KEY), str):
        return Lambda(value)
    return obj
