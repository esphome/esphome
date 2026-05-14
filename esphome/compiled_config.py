"""Validated-config cache for the upload/logs fast path.

compile dumps the validated config to <data_dir>/storage/<file>.validated.yaml;
the next upload/logs for that YAML reuses it instead of running the full
read_config pipeline. YAML round-trip (yaml_util.dump/load_yaml) keeps
!lambda/!include/IDs/paths intact; mtime gates staleness.
"""

from __future__ import annotations

import logging
from pathlib import Path

from esphome.core import CORE
from esphome.helpers import write_file
from esphome.storage_json import StorageJSON, ext_storage_path
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)


def compiled_config_path(config_filename: str) -> Path:
    """Path to the cached validated config alongside the storage sidecar."""
    return CORE.data_dir / "storage" / f"{config_filename}.validated.yaml"


def _cache_is_fresh(cache_path: Path, source_path: Path) -> bool:
    """True iff the cache file exists and isn't older than the source."""
    try:
        return cache_path.stat().st_mtime >= source_path.stat().st_mtime
    except OSError:
        return False


def save_compiled_config(config: ConfigType) -> None:
    """Write the validated-config cache. Always-write so mtime stays fresh.

    Mode 0600 because show_secrets=True resolves !secret inline.
    Failures are non-fatal: the fast path falls back to read_config.
    """
    from esphome import yaml_util

    try:
        rendered = yaml_util.dump(config, show_secrets=True)
        write_file(compiled_config_path(CORE.config_filename), rendered, private=True)
    except Exception as err:  # pylint: disable=broad-except
        _LOGGER.debug("Skipping compiled config cache write: %s", err)


def load_compiled_config(conf_path: Path) -> ConfigType | None:
    """Load the cached validated config and apply storage metadata to CORE.

    Returns None (caller falls back to read_config) when the cache is
    missing, older than the source YAML, unparseable, or the sidecar
    is incomplete.
    """
    cache_path = compiled_config_path(conf_path.name)
    if not _cache_is_fresh(cache_path, conf_path):
        return None

    from esphome import yaml_util

    try:
        config = yaml_util.load_yaml(cache_path, clear_secrets=False)
    except Exception:  # pylint: disable=broad-except
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
