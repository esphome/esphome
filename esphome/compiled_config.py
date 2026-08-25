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
from esphome.core import CORE, EsphomeError, Lambda
from esphome.helpers import write_file
from esphome.storage_json import StorageJSON, ext_storage_path, storage_path
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
        # Likely persistent (permissions, full disk): every upload/logs
        # pays the slow path until it clears, so surface it.
        _LOGGER.warning("Skipping compiled config cache write: %s", err)


def save_compiled_config_and_sidecar(config: ConfigType) -> None:
    """Refresh the cache from the upload/logs fallback (CORE.config must be set).

    The cache is only written when a complete sidecar is on disk:
    load_compiled_config can't use it otherwise, and it holds resolved
    secrets.
    """
    if _refresh_sidecar():
        save_compiled_config(config)


def _refresh_sidecar() -> bool:
    """Ensure a complete sidecar is on disk; True when one is.

    Writes one (without claiming a build) when missing or wizard-only.
    Failures are non-fatal; the next upload/logs pays the slow path again.
    """
    try:
        path = storage_path()
        try:
            old = StorageJSON.load_strict(path)
        except Exception as err:  # noqa: BLE001  # pylint: disable=broad-except
            # Present but unreadable: it may hold a real build's metadata,
            # and a fresh rewrite would also stop the next compile from
            # cleaning a possibly incoherent build tree.
            _LOGGER.warning(
                "Not caching: storage sidecar %s is unreadable (%s)", path, err
            )
            return False
        if old is not None and old.can_apply_to_core():
            # Compile-written; nothing to refresh.
            return True
        if CORE.build_path is not None and CORE.build_path.exists():
            # An unvalidated build tree: its absent or mismatched sidecar
            # is what makes the next compile wipe it, so don't vouch for
            # a build this run never saw.
            _LOGGER.warning(
                "Not caching: build tree %s has no matching sidecar; "
                "'esphome compile' will settle it",
                CORE.build_path,
            )
            return False
        new = StorageJSON.from_esphome_core(CORE, old, claim_build=False)
        if not new.can_apply_to_core():
            _LOGGER.warning("Not caching: rebuilt storage sidecar is still incomplete")
            return False
        new.save(path)
        return True
    except (OSError, EsphomeError) as err:
        # write_file wraps OSError into EsphomeError. Persistent
        # (unwritable storage dir), so surface that every upload/logs
        # pays the slow path.
        _LOGGER.warning("Could not refresh the storage sidecar: %s", err)
    except Exception:  # noqa: BLE001  # pylint: disable=broad-except
        # A structural bug; keep the traceback so it isn't mistaken
        # for the I/O failure above.
        _LOGGER.warning(
            "Unexpected error refreshing the storage sidecar", exc_info=True
        )
    return False


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
    if storage is None or not storage.can_apply_to_core():
        _LOGGER.debug("Ignoring compiled config cache: sidecar missing or incomplete")
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
