"""Tests for the `--from-storage-json` validated-config cache fast path."""

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
    CONF_NAME,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
)
from esphome.core import CORE
from esphome.storage_json import StorageJSON, compiled_config_path, load_compiled_config

_VALIDATED_CONFIG_YAML = """\
esphome:
  name: lite_test
  friendly_name: Lite Test Device
esp32:
  board: nodemcu-32s
logger:
  baud_rate: 115200
api:
  port: 6053
  encryption:
    key: 6dGhpcyBpcyBhIHRlc3Q=
ota:
  - platform: esphome
    port: 3232
    password: secret
wifi:
  ssid: ssid
  use_address: 192.168.1.42
"""


def _write_storage(storage_path: Path) -> None:
    """Write a vanilla StorageJSON sidecar for the cache tests."""
    storage_path.parent.mkdir(parents=True, exist_ok=True)
    data = {
        "storage_version": 1,
        "name": "lite_test",
        "friendly_name": "Lite Test Device",
        "comment": None,
        "esphome_version": "2026.1.0",
        "src_version": 1,
        "address": "192.168.1.42",
        "web_port": None,
        "esp_platform": "ESP32",
        "build_path": "/build/lite_test",
        "firmware_bin_path": "/build/lite_test/firmware.bin",
        "loaded_integrations": ["api", "logger", "ota", "wifi"],
        "loaded_platforms": [],
        "no_mdns": False,
        "framework": "arduino",
        "core_platform": "esp32",
    }
    storage_path.write_text(json.dumps(data))


@pytest.fixture
def fresh_cache_files(tmp_path: Path) -> Path:
    """Set up a YAML + StorageJSON + validated-config cache.

    Cache mtime is bumped 5s past the YAML so the staleness check
    treats it as fresh.
    """
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")
    CORE.config_path = yaml_path

    storage_dir = tmp_path / ".esphome" / "storage"
    _write_storage(storage_dir / "lite_test.yaml.json")

    cache_path = storage_dir / "lite_test.yaml.validated.yaml"
    cache_path.write_text(_VALIDATED_CONFIG_YAML)
    yaml_stat = yaml_path.stat()
    os.utime(cache_path, (yaml_stat.st_atime, yaml_stat.st_mtime + 5))

    return yaml_path


def test_compiled_config_path_lives_alongside_sidecar(setup_core: Path) -> None:
    """The cache file shape is predictable from the YAML filename."""
    path = compiled_config_path("device.yaml")
    assert str(path).endswith("storage/device.yaml.validated.yaml")


def test_load_compiled_config_happy_path(fresh_cache_files: Path) -> None:
    """Fresh cache → returns the validated config dict."""
    config = load_compiled_config(fresh_cache_files)

    assert config is not None
    assert config[CONF_ESPHOME][CONF_NAME] == "lite_test"
    assert config[CONF_API]["encryption"]["key"] == "6dGhpcyBpcyBhIHRlc3Q="
    assert config["ota"][0]["password"] == "secret"


def test_load_compiled_config_missing_cache(tmp_path: Path) -> None:
    """No cache file on disk → None so caller falls back."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")
    CORE.config_path = yaml_path

    assert load_compiled_config(yaml_path) is None


def test_load_compiled_config_stale_cache(tmp_path: Path) -> None:
    """Cache older than the YAML → None (the YAML was edited)."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")
    CORE.config_path = yaml_path

    storage_dir = tmp_path / ".esphome" / "storage"
    storage_dir.mkdir(parents=True, exist_ok=True)
    cache_path = storage_dir / "lite_test.yaml.validated.yaml"
    cache_path.write_text(_VALIDATED_CONFIG_YAML)

    yaml_stat = yaml_path.stat()
    # Cache is one minute older than the YAML — the YAML's been edited
    # since the last compile, so the cache no longer describes the
    # binary on disk.
    os.utime(cache_path, (yaml_stat.st_atime, yaml_stat.st_mtime - 60))

    assert load_compiled_config(yaml_path) is None


def test_load_compiled_config_corrupt_cache(tmp_path: Path) -> None:
    """Cache file is unparseable → None so caller falls back."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")
    CORE.config_path = yaml_path

    storage_dir = tmp_path / ".esphome" / "storage"
    storage_dir.mkdir(parents=True, exist_ok=True)
    cache_path = storage_dir / "lite_test.yaml.validated.yaml"
    cache_path.write_text("not: valid: yaml: [")

    yaml_stat = yaml_path.stat()
    os.utime(cache_path, (yaml_stat.st_atime, yaml_stat.st_mtime + 5))

    assert load_compiled_config(yaml_path) is None


def test_storage_json_apply_to_core_populates_target_platform(tmp_path: Path) -> None:
    """apply_to_core sets the CORE attributes upload / logs read."""
    storage_path = tmp_path / "lite_test.yaml.json"
    _write_storage(storage_path)
    storage = StorageJSON.load(storage_path)
    assert storage is not None

    storage.apply_to_core()

    assert CORE.name == "lite_test"
    assert CORE.friendly_name == "Lite Test Device"
    assert CORE.build_path == Path("/build/lite_test")
    assert CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] == "esp32"
    assert CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] == "arduino"
    assert "api" in CORE.loaded_integrations


@pytest.mark.parametrize("command", ["upload", "logs"])
def test_run_esphome_from_storage_json_skips_read_config(
    command: str, fresh_cache_files: Path
) -> None:
    """`--from-storage-json` makes the dispatcher skip read_config()."""
    yaml_path = fresh_cache_files

    captured = {}

    def _stub(_args, config):
        captured["config"] = config
        return 0

    with (
        patch("esphome.__main__.read_config") as mock_read,
        patch.dict("esphome.__main__.POST_CONFIG_ACTIONS", {command: _stub}),
    ):
        result = run_esphome(
            ["esphome", command, "--from-storage-json", str(yaml_path)]
        )

    mock_read.assert_not_called()
    assert result == 0
    # The dispatcher hands the cached config dict through unchanged.
    assert captured["config"][CONF_ESPHOME][CONF_NAME] == "lite_test"
    assert captured["config"][CONF_API]["encryption"]["key"] == "6dGhpcyBpcyBhIHRlc3Q="


@pytest.mark.parametrize("command", ["upload", "logs"])
def test_run_esphome_from_storage_json_falls_back_when_missing(
    tmp_path: Path, command: str
) -> None:
    """With no cache on disk, the dispatcher falls back to read_config()."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")

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
    assert result == 2


def test_run_esphome_from_storage_json_falls_back_when_stale(
    tmp_path: Path,
) -> None:
    """If YAML mtime > cache mtime, dispatcher falls back to read_config()."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")

    storage_dir = tmp_path / ".esphome" / "storage"
    _write_storage(storage_dir / "lite_test.yaml.json")

    cache_path = storage_dir / "lite_test.yaml.validated.yaml"
    cache_path.write_text(_VALIDATED_CONFIG_YAML)
    yaml_stat = yaml_path.stat()
    os.utime(cache_path, (yaml_stat.st_atime, yaml_stat.st_mtime - 60))

    with (
        patch("esphome.__main__.read_config", return_value=None) as mock_read,
        patch.dict(
            "esphome.__main__.POST_CONFIG_ACTIONS",
            {"upload": lambda args, config: 0},
        ),
    ):
        run_esphome(["esphome", "upload", "--from-storage-json", str(yaml_path)])

    mock_read.assert_called_once()


def test_run_esphome_without_flag_still_calls_read_config(
    fresh_cache_files: Path,
) -> None:
    """Sanity: omitting the flag preserves the current behaviour."""
    yaml_path = fresh_cache_files

    with (
        patch("esphome.__main__.read_config", return_value=None) as mock_read,
        patch.dict(
            "esphome.__main__.POST_CONFIG_ACTIONS",
            {"upload": lambda args, config: 0},
        ),
    ):
        run_esphome(["esphome", "upload", str(yaml_path)])

    mock_read.assert_called_once()
