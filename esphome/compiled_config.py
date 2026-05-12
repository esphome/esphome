"""Validated-config cache for the ``upload`` / ``logs`` fast path.

After every successful ``esphome compile``, the writer dumps the
validated config to ``<data_dir>/storage/<file>.validated.yaml``.
The next ``esphome upload`` / ``esphome logs`` for that YAML reuses
the cache instead of re-running the full ``read_config()`` pipeline
(parse + schema validate + final-validate + external-component
refresh), which is dead work once the binary on disk is known good.

The cache uses YAML (via ``yaml_util``) rather than JSON so all
esphome-specific tags (``!lambda``, ``!include``, ``ID`` instances,
paths) round-trip cleanly, and so the small ``StorageJSON`` sidecar
isn't bloated by configs that can grow past a megabyte. Staleness
is gated by an mtime check against the source YAML; the loader
falls back to ``None`` (and the caller to ``read_config()``) whenever
the cache is missing, stale, unparseable, or the companion
``StorageJSON`` sidecar can't be loaded.
"""

from __future__ import annotations

import logging
from pathlib import Path

from esphome.core import CORE
from esphome.helpers import write_file_if_changed
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
    """Dump the validated config to its sidecar YAML file.

    Called from the writer after every successful compile so the next
    ``esphome upload`` / ``esphome logs`` for this YAML can skip
    validation via :func:`load_compiled_config`. Failures here are
    non-fatal: the worst case is that the fast path falls back to a
    full ``read_config`` next time.
    """
    from esphome import yaml_util

    try:
        # show_secrets=True keeps the cache self-contained; it lives
        # in the same trust zone as the rest of .esphome/storage/.
        rendered = yaml_util.dump(config, show_secrets=True)
        write_file_if_changed(compiled_config_path(CORE.config_filename), rendered)
    except Exception as err:  # pylint: disable=broad-except
        _LOGGER.debug("Skipping compiled config cache write: %s", err)


def load_compiled_config(conf_path: Path) -> ConfigType | None:
    """Load the cached validated config and apply storage metadata to CORE.

    Single entry point for the ``upload`` / ``logs`` fast path: loads
    the cache, applies the ``StorageJSON`` sidecar's platform / build
    metadata to ``CORE``, and returns the config dict. Returns
    ``None`` (so the caller falls back to ``read_config``) when the
    cache is missing, older than the source YAML, unparseable, or
    when the sidecar can't be loaded. The mtime check catches the
    common "user edited the YAML and forgot to recompile" case;
    deeper drift (an edited ``!include`` whose parent YAML mtime
    didn't change) is the user's responsibility.
    """
    cache_path = compiled_config_path(conf_path.name)
    if not _cache_is_fresh(cache_path, conf_path):
        return None

    from esphome import yaml_util

    try:
        # clear_secrets=False keeps in-flight secret state intact; the
        # cache is self-contained and resolves no !secret references.
        config = yaml_util.load_yaml(cache_path, clear_secrets=False)
    except Exception:  # pylint: disable=broad-except
        return None

    storage = StorageJSON.load(ext_storage_path(conf_path.name))
    if storage is None:
        return None
    # `apply_to_core` assumes the sidecar was written by `from_esphome_core`
    # after a real compile, which always sets at least one of these. A
    # wizard-only sidecar (no compile) can't drive upload/logs.
    if not storage.core_platform and not storage.target_platform:
        return None
    storage.apply_to_core()
    return config
