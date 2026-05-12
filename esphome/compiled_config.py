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
the cache is missing, stale, unparseable, or structurally incomplete.
"""

from __future__ import annotations

import logging
from pathlib import Path

from esphome.const import (
    CONF_BUILD_PATH,
    CONF_ESPHOME,
    CONF_FRAMEWORK,
    CONF_FRIENDLY_NAME,
    CONF_NAME,
    CONF_TYPE,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
)
from esphome.core import CORE
from esphome.helpers import write_file_if_changed
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)


def _populate_core_from_validated_config(config: ConfigType) -> None:
    """Set up ``CORE`` from an already-validated config dict.

    Reads from the same config keys ``preload_core_config`` and the
    target-platform component validator would write into during a full
    ``read_config`` -- using the cached config dict as the canonical
    source instead of duplicating the field list in a separate sidecar
    schema. ``CORE.address`` and ``CORE.web_port`` aren't set here:
    they're already properties that derive themselves from
    ``CORE.config`` on access, which the dispatcher assigns next.
    """
    from esphome.core.config import _is_target_platform

    esphome_block = config[CONF_ESPHOME]
    CORE.name = esphome_block[CONF_NAME]
    CORE.friendly_name = esphome_block.get(CONF_FRIENDLY_NAME)
    if (build_path := esphome_block.get(CONF_BUILD_PATH)) is not None:
        CORE.build_path = CORE.data_dir / build_path

    CORE.data.setdefault(KEY_CORE, {})
    for domain, sub in config.items():
        if not isinstance(domain, str) or not _is_target_platform(domain):
            continue
        CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = domain
        # Every platform component follows the
        # `<platform>.framework.type` convention to express which
        # framework the binary was built against (esp-idf, arduino,
        # zephyr). A platform without a framework block (host) just
        # leaves KEY_TARGET_FRAMEWORK unset, matching what
        # `read_config` would do.
        if (
            isinstance(sub, dict)
            and isinstance((framework := sub.get(CONF_FRAMEWORK)), dict)
            and (framework_type := framework.get(CONF_TYPE)) is not None
        ):
            CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = framework_type
        break


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
    """Load the cached validated config and set up ``CORE``.

    Single entry point for the ``upload`` / ``logs`` fast path. Returns
    ``None`` (so the caller falls back to ``read_config``) when the
    cache is missing, older than the source YAML, unparseable, or
    structurally incomplete. The mtime check catches the common "user
    edited the YAML and forgot to recompile" case; deeper drift (an
    edited ``!include`` whose parent YAML mtime didn't change) is the
    user's responsibility.

    CORE state is derived from the cached config dict directly --
    same source ``read_config`` uses -- so there's no separate sidecar
    schema to keep in sync.
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

    if not isinstance(config, dict) or CONF_ESPHOME not in config:
        return None

    try:
        _populate_core_from_validated_config(config)
    except (KeyError, TypeError):
        return None
    return config
