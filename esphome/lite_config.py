"""Lite-config loader for ``upload`` / ``logs`` fast paths.

When the caller (typically a dashboard) already has a known-good
firmware binary on disk and only needs the CLI to ship bytes to a
device or stream logs back, running the full ``read_config()``
pipeline is dead work. It re-parses every ``!include``, runs every
component's schema validator, executes all final-validate hooks, and
fetches/refreshes external components -- producing a fully validated
``Config`` object that the upload / logs subcommands only consult for a
handful of leaf keys.

This module loads the per-config ``StorageJSON`` sidecar that was
written by the last successful ``compile`` run, populates ``CORE`` with
the platform / build metadata from it, and re-parses just enough of the
YAML head (substitutions + packages, no schema validation) to recover
``api:`` / ``logger:`` / ``ota:`` / ``mqtt:`` / network blocks for the
subcommand callers.

If the sidecar is missing or older than the YAML, this returns ``None``
so the dispatcher can fall back to the full ``read_config()`` path.
That makes the flag a pure optimisation: a cold cache never produces a
worse outcome than today.
"""

from __future__ import annotations

import logging
from pathlib import Path
from typing import Any

from esphome.const import (
    CONF_API,
    CONF_ESPHOME,
    CONF_ETHERNET,
    CONF_FRIENDLY_NAME,
    CONF_LOGGER,
    CONF_MQTT,
    CONF_NAME,
    CONF_OPENTHREAD,
    CONF_OTA,
    CONF_USE_ADDRESS,
    CONF_WEB_SERVER,
    CONF_WIFI,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
)
from esphome.core import CORE
from esphome.storage_json import StorageJSON, ext_storage_path
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

# Top-level keys we copy from the raw YAML into the lite config dict.
# Anything outside this list is irrelevant to ``upload`` / ``logs``.
_LITE_TOP_LEVEL_KEYS: tuple[str, ...] = (
    CONF_ESPHOME,
    CONF_API,
    CONF_LOGGER,
    CONF_OTA,
    CONF_MQTT,
    CONF_WIFI,
    CONF_ETHERNET,
    CONF_OPENTHREAD,
    CONF_WEB_SERVER,
)


def _parse_yaml_head(
    conf_path: Path, command_line_substitutions: dict[str, Any] | None
) -> dict[str, Any] | None:
    """Load and lightly process the YAML, without schema validation.

    Resolves ``!include`` / ``!secret`` (via ``yaml_util``),
    substitutions, and ``packages:`` -- the three passes whose output
    the upload / logs subcommands need. Returns ``None`` on any error
    so the caller can fall back to the full ``read_config()`` path.
    """
    from esphome import yaml_util
    from esphome.components.packages import resolve_packages
    from esphome.components.substitutions import do_substitution_pass

    try:
        config = yaml_util.load_yaml(conf_path)
    except Exception:  # pylint: disable=broad-except
        return None

    try:
        config = do_substitution_pass(config, command_line_substitutions)
    except Exception:  # pylint: disable=broad-except
        return None

    try:
        config = resolve_packages(
            config, command_line_substitutions=command_line_substitutions
        )
    except Exception:  # pylint: disable=broad-except
        return None

    return config


def _build_lite_config(raw_config: dict[str, Any]) -> ConfigType:
    """Project the raw YAML down to the keys upload / logs read."""
    return {key: raw_config[key] for key in _LITE_TOP_LEVEL_KEYS if key in raw_config}


def _populate_core_from_storage(storage: StorageJSON) -> None:
    """Populate ``CORE`` fields the subcommands read off the platform."""
    CORE.name = storage.name
    CORE.friendly_name = storage.friendly_name
    CORE.build_path = storage.build_path
    CORE.loaded_integrations = set(storage.loaded_integrations)
    CORE.loaded_platforms = set(storage.loaded_platforms)

    core_platform = storage.core_platform or (
        storage.target_platform.lower() if storage.target_platform else None
    )
    CORE.data.setdefault(KEY_CORE, {})
    if core_platform is not None:
        CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = core_platform
    if storage.framework is not None:
        CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = storage.framework


def _ensure_address_in_config(config: ConfigType, storage: StorageJSON) -> None:
    """Make sure ``CORE.address`` resolves once the lite config is set.

    ``CORE.address`` reads ``use_address`` off the wifi / ethernet /
    openthread block; if the dashboard rewrote the YAML between the
    last compile and now those blocks may be missing from the lite
    parse. As a backstop, lift the address from the StorageJSON when
    we have one and the YAML doesn't supply one already.
    """
    if storage.address is None:
        return
    for network_type in (CONF_WIFI, CONF_ETHERNET, CONF_OPENTHREAD):
        if network_type in config and CONF_USE_ADDRESS in config[network_type]:
            return
    # Fall back to a synthetic wifi block so ``CORE.address`` resolves.
    config.setdefault(CONF_WIFI, {})[CONF_USE_ADDRESS] = storage.address


def load_lite_config_from_storage(
    conf_path: Path, command_line_substitutions: dict[str, Any] | None = None
) -> ConfigType | None:
    """Build a minimal config + populate CORE from the StorageJSON sidecar.

    Returns the lite config dict on success, or ``None`` when the
    dispatcher should fall back to ``read_config()`` (sidecar missing,
    stale, unreadable, or required fields absent).
    """
    storage_path = ext_storage_path(conf_path.name)
    try:
        yaml_stat = conf_path.stat()
    except OSError:
        return None
    try:
        storage_stat = storage_path.stat()
    except OSError:
        return None

    # The sidecar is written by `compile`; if the YAML has been edited
    # since, the cached platform / loaded_integrations may no longer
    # describe what the binary on disk was built from. Fall back to a
    # full validation pass in that case.
    if storage_stat.st_mtime < yaml_stat.st_mtime:
        return None

    storage = StorageJSON.load(storage_path)
    if storage is None:
        return None
    if not storage.target_platform and not storage.core_platform:
        # An incomplete sidecar (e.g. from a wizard run that never
        # compiled) can't drive the upload / logs subcommands.
        return None

    raw_config = _parse_yaml_head(conf_path, command_line_substitutions)
    if raw_config is None:
        return None
    if CONF_ESPHOME not in raw_config or CONF_NAME not in raw_config[CONF_ESPHOME]:
        return None

    config = _build_lite_config(raw_config)

    # Backfill `esphome:` block fields from the sidecar so downstream
    # callers that read `config["esphome"]["name"]` work even when the
    # YAML uses substitutions that didn't survive the light parse.
    esphome_block = config.setdefault(CONF_ESPHOME, {})
    esphome_block.setdefault(CONF_NAME, storage.name)
    if storage.friendly_name is not None:
        esphome_block.setdefault(CONF_FRIENDLY_NAME, storage.friendly_name)

    _populate_core_from_storage(storage)
    _ensure_address_in_config(config, storage)

    return config
