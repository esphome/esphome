"""Tests for the StorageJSON-backed lite config fast path."""

from __future__ import annotations

import json
import os
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.__main__ import run_esphome
from esphome.const import (
    CONF_API,
    CONF_ESPHOME,
    CONF_LOGGER,
    CONF_NAME,
    CONF_OTA,
    CONF_USE_ADDRESS,
    CONF_WIFI,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
)
from esphome.core import CORE
from esphome.lite_config import load_lite_config_from_storage

# A YAML the lite parser can load. Contains every block ``upload`` /
# ``logs`` cares about (api, logger, ota, wifi) plus a substitution so
# the tests exercise the substitution pass too.
_SAMPLE_YAML = """\
substitutions:
  device_name: lite_test
  encryption_key: 6dGhpcyBpcyBhIHRlc3Q=

esphome:
  name: ${device_name}
  friendly_name: Lite Test Device

esp32:
  board: nodemcu-32s

logger:

api:
  encryption:
    key: ${encryption_key}

ota:
  - platform: esphome

wifi:
  ssid: "ssid"
  password: "password"
  use_address: 192.168.1.42
"""


def _write_storage(
    storage_path: Path,
    *,
    name: str = "lite_test",
    friendly_name: str | None = "Lite Test Device",
    address: str | None = "192.168.1.42",
    target_platform: str = "ESP32",
    core_platform: str | None = "esp32",
    framework: str | None = "arduino",
    build_path: str | None = "/build/lite_test",
    firmware_bin_path: str | None = "/build/lite_test/firmware.bin",
    loaded_integrations: list[str] | None = None,
    loaded_platforms: list[str] | None = None,
) -> None:
    """Write a StorageJSON sidecar to ``storage_path`` for the tests."""
    storage_path.parent.mkdir(parents=True, exist_ok=True)
    data = {
        "storage_version": 1,
        "name": name,
        "friendly_name": friendly_name,
        "comment": None,
        "esphome_version": "2026.1.0",
        "src_version": 1,
        "address": address,
        "web_port": None,
        "esp_platform": target_platform,
        "build_path": build_path,
        "firmware_bin_path": firmware_bin_path,
        "loaded_integrations": loaded_integrations or ["api", "logger", "ota", "wifi"],
        "loaded_platforms": loaded_platforms or [],
        "no_mdns": False,
        "framework": framework,
        "core_platform": core_platform,
    }
    storage_path.write_text(json.dumps(data))


@pytest.fixture
def lite_config_files(tmp_path: Path) -> tuple[Path, Path]:
    """Create a YAML + matching StorageJSON for the lite-config tests.

    The YAML lives at ``<tmp>/lite_test.yaml`` so ``CORE.data_dir``
    points into ``<tmp>/.esphome``. The storage sidecar is written
    after the YAML and then back-dated, so callers can rely on
    ``storage.mtime >= yaml.mtime`` matching the production happy
    path (compile writes the sidecar last).
    """
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text(_SAMPLE_YAML)
    CORE.config_path = yaml_path

    storage_path = tmp_path / ".esphome" / "storage" / "lite_test.yaml.json"
    _write_storage(storage_path)
    # Storage is younger than the YAML by virtue of being created last,
    # but make the relationship explicit for tests that depend on it.
    yaml_stat = yaml_path.stat()
    os.utime(storage_path, (yaml_stat.st_atime, yaml_stat.st_mtime + 5))

    return yaml_path, storage_path


def test_lite_config_happy_path(lite_config_files: tuple[Path, Path]) -> None:
    """Storage + YAML in sync: lite config returns a populated dict and CORE."""
    yaml_path, _ = lite_config_files

    config = load_lite_config_from_storage(yaml_path, {})

    assert config is not None
    assert config[CONF_ESPHOME][CONF_NAME] == "lite_test"
    assert CONF_LOGGER in config
    assert CONF_API in config
    assert config[CONF_API]["encryption"]["key"] == "6dGhpcyBpcyBhIHRlc3Q="
    assert CONF_OTA in config
    assert config[CONF_OTA][0]["platform"] == "esphome"
    # Lite config should NOT contain bulky non-essential keys like esp32.
    assert "esp32" not in config

    # CORE populated from storage.
    assert CORE.name == "lite_test"
    assert CORE.friendly_name == "Lite Test Device"
    assert CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] == "esp32"
    assert CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] == "arduino"
    assert CORE.build_path == Path("/build/lite_test")
    assert "api" in CORE.loaded_integrations


def test_lite_config_missing_storage_returns_none(tmp_path: Path) -> None:
    """No StorageJSON sidecar → return None so caller falls back."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text(_SAMPLE_YAML)
    CORE.config_path = yaml_path

    assert load_lite_config_from_storage(yaml_path, {}) is None


def test_lite_config_stale_storage_returns_none(tmp_path: Path) -> None:
    """StorageJSON older than YAML → return None.

    The YAML may have grown a new ``api:`` key since the binary was
    built; falling back to ``read_config()`` catches that.
    """
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text(_SAMPLE_YAML)
    CORE.config_path = yaml_path

    storage_path = tmp_path / ".esphome" / "storage" / "lite_test.yaml.json"
    _write_storage(storage_path)

    yaml_stat = yaml_path.stat()
    # Storage is one minute older than the YAML.
    os.utime(storage_path, (yaml_stat.st_atime, yaml_stat.st_mtime - 60))

    assert load_lite_config_from_storage(yaml_path, {}) is None


def test_lite_config_address_backfilled_from_storage(tmp_path: Path) -> None:
    """No wifi/ethernet block in YAML → address comes from storage.

    Some users gate their network blocks behind substitutions that
    don't fire in the lite parse. The loader should fall back to the
    sidecar's ``address`` so ``CORE.address`` still resolves.
    """
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text(
        "esphome:\n  name: lite_test\nesp32:\n  board: nodemcu-32s\n"
        "logger:\napi:\nota:\n  - platform: esphome\n"
    )
    CORE.config_path = yaml_path

    storage_path = tmp_path / ".esphome" / "storage" / "lite_test.yaml.json"
    _write_storage(storage_path, address="10.0.0.5")

    yaml_stat = yaml_path.stat()
    os.utime(storage_path, (yaml_stat.st_atime, yaml_stat.st_mtime + 5))

    config = load_lite_config_from_storage(yaml_path, {})
    assert config is not None
    assert config[CONF_WIFI][CONF_USE_ADDRESS] == "10.0.0.5"


def test_lite_config_corrupt_storage_returns_none(tmp_path: Path) -> None:
    """A malformed sidecar (StorageJSON.load returns None) → fall back."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text(_SAMPLE_YAML)
    CORE.config_path = yaml_path

    storage_path = tmp_path / ".esphome" / "storage" / "lite_test.yaml.json"
    storage_path.parent.mkdir(parents=True, exist_ok=True)
    storage_path.write_text("not valid json{")

    yaml_stat = yaml_path.stat()
    os.utime(storage_path, (yaml_stat.st_atime, yaml_stat.st_mtime + 5))

    assert load_lite_config_from_storage(yaml_path, {}) is None


@pytest.mark.parametrize("command", ["upload", "logs"])
def test_run_esphome_from_storage_json_skips_read_config(
    tmp_path: Path,
    command: str,
    lite_config_files: tuple[Path, Path],
) -> None:
    """`--from-storage-json` makes the dispatcher skip read_config()."""
    yaml_path, _ = lite_config_files

    # Stub out the subcommand handler so the test doesn't try to open
    # a network connection or run platform-specific upload logic.
    with (
        patch("esphome.__main__.read_config") as mock_read,
        patch.dict(
            "esphome.__main__.POST_CONFIG_ACTIONS",
            {command: lambda args, config: 0},
        ),
    ):
        result = run_esphome(
            ["esphome", command, "--from-storage-json", str(yaml_path)]
        )

    mock_read.assert_not_called()
    assert result == 0


@pytest.mark.parametrize("command", ["upload", "logs"])
def test_run_esphome_from_storage_json_falls_back_when_missing(
    tmp_path: Path, command: str
) -> None:
    """With no sidecar on disk, the dispatcher falls back to read_config()."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text(_SAMPLE_YAML)

    with (
        patch("esphome.__main__.read_config", return_value=None) as mock_read,
        patch.dict(
            "esphome.__main__.POST_CONFIG_ACTIONS",
            {command: lambda args, config: 0},
        ),
    ):
        result = run_esphome(
            ["esphome", command, "--from-storage-json", str(yaml_path)]
        )

    mock_read.assert_called_once()
    # read_config returned None → dispatcher exits with 2.
    assert result == 2


def test_run_esphome_without_flag_still_calls_read_config(
    tmp_path: Path, lite_config_files: tuple[Path, Path]
) -> None:
    """Sanity: omitting the flag preserves the current behaviour."""
    yaml_path, _ = lite_config_files

    with (
        patch("esphome.__main__.read_config", return_value=None) as mock_read,
        patch.dict(
            "esphome.__main__.POST_CONFIG_ACTIONS",
            {"upload": lambda args, config: 0},
        ),
    ):
        run_esphome(["esphome", "upload", str(yaml_path)])

    mock_read.assert_called_once()
