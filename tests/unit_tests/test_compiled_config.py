"""Tests for the validated-config cache used by upload/logs."""

from __future__ import annotations

import json
import os
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.__main__ import run_esphome
from esphome.compiled_config import (
    compiled_config_path,
    load_compiled_config,
    save_compiled_config,
)
from esphome.const import (
    CONF_API,
    CONF_ESPHOME,
    CONF_NAME,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
)
from esphome.core import CORE

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


def _write_cache(cache_path: Path, body: str = _VALIDATED_CONFIG_YAML) -> Path:
    """Write the cache file and return it."""
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    cache_path.write_text(body)
    return cache_path


def _set_cache_mtime(cache_path: Path, yaml_path: Path, *, offset: int) -> None:
    """Force the cache file's mtime relative to the source YAML.

    Positive offset → cache is fresh. Negative → cache is stale.
    """
    yaml_stat = yaml_path.stat()
    os.utime(cache_path, (yaml_stat.st_atime, yaml_stat.st_mtime + offset))


@pytest.fixture
def fresh_cache_files(tmp_path: Path) -> Path:
    """YAML + StorageJSON + cache, all consistent and fresh."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")
    CORE.config_path = yaml_path

    storage_dir = tmp_path / ".esphome" / "storage"
    _write_storage(storage_dir / "lite_test.yaml.json")
    cache = _write_cache(storage_dir / "lite_test.yaml.validated.yaml")
    _set_cache_mtime(cache, yaml_path, offset=5)

    return yaml_path


def test_compiled_config_path_lives_alongside_sidecar(setup_core: Path) -> None:
    """The cache file shape is predictable from the YAML filename."""
    path = compiled_config_path("device.yaml")
    assert path.name == "device.yaml.validated.yaml"
    assert path.parent.name == "storage"


def test_load_compiled_config_happy_path(fresh_cache_files: Path) -> None:
    """Fresh cache + sidecar → returns config and populates CORE."""
    config = load_compiled_config(fresh_cache_files)

    assert config is not None
    assert config[CONF_ESPHOME][CONF_NAME] == "lite_test"
    assert config[CONF_API]["encryption"]["key"] == "6dGhpcyBpcyBhIHRlc3Q="
    assert config["ota"][0]["password"] == "secret"

    # apply_to_core populated exactly what upload/logs read off CORE.
    assert CORE.name == "lite_test"
    assert CORE.build_path == Path("/build/lite_test")
    assert CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] == "esp32"
    assert CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] == "arduino"


@pytest.mark.parametrize(
    "scenario",
    ["missing_cache", "stale_cache", "corrupt_cache", "missing_sidecar"],
)
def test_load_compiled_config_falls_back(tmp_path: Path, scenario: str) -> None:
    """All non-happy cases return None so the caller falls back."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")
    CORE.config_path = yaml_path
    storage_dir = tmp_path / ".esphome" / "storage"
    cache_path = storage_dir / "lite_test.yaml.validated.yaml"
    sidecar_path = storage_dir / "lite_test.yaml.json"

    if scenario == "missing_cache":
        pass  # no cache, no sidecar
    elif scenario == "stale_cache":
        _write_storage(sidecar_path)
        _set_cache_mtime(_write_cache(cache_path), yaml_path, offset=-60)
    elif scenario == "corrupt_cache":
        _write_storage(sidecar_path)
        _set_cache_mtime(
            _write_cache(cache_path, "not: valid: yaml: ["), yaml_path, offset=5
        )
    elif scenario == "missing_sidecar":
        # Cache fresh + parseable, but no StorageJSON → can't populate CORE.
        _set_cache_mtime(_write_cache(cache_path), yaml_path, offset=5)

    assert load_compiled_config(yaml_path) is None


@pytest.mark.parametrize("command", ["upload", "logs"])
def test_run_esphome_upload_and_logs_use_cache_when_fresh(
    command: str,
    fresh_cache_files: Path,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """upload/logs skip read_config() when the cache is fresh."""
    captured: dict = {}

    def _stub(_args, config):
        captured["config"] = config
        return 0

    with (
        caplog.at_level("INFO", logger="esphome.__main__"),
        patch("esphome.__main__.read_config") as mock_read,
        patch.dict("esphome.__main__.POST_CONFIG_ACTIONS", {command: _stub}),
    ):
        assert run_esphome(["esphome", command, str(fresh_cache_files)]) == 0

    mock_read.assert_not_called()
    assert captured["config"][CONF_ESPHOME][CONF_NAME] == "lite_test"
    assert captured["config"][CONF_API]["encryption"]["key"] == "6dGhpcyBpcyBhIHRlc3Q="
    # The success-branch log line is part of the patch; assert on it so
    # branch coverage stays unambiguous in CI.
    assert "Loaded validated config cache" in caplog.text


@pytest.mark.parametrize("command", ["upload", "logs"])
def test_run_esphome_upload_and_logs_fall_back_when_no_cache(
    tmp_path: Path, command: str
) -> None:
    """Without a cache, the dispatcher falls back to read_config()."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")

    with (
        patch("esphome.__main__.read_config", return_value=None) as mock_read,
        patch.dict(
            "esphome.__main__.POST_CONFIG_ACTIONS",
            {command: lambda args, config: 0},
        ),
    ):
        assert run_esphome(["esphome", command, str(yaml_path)]) == 2

    mock_read.assert_called_once()


def test_run_esphome_upload_with_substitution_skips_cache(
    fresh_cache_files: Path,
) -> None:
    """`-s key value` forces a fresh validation -- the cache was written
    against the prior substitution set, so reusing it would silently
    ignore the override."""
    with (
        patch("esphome.__main__.read_config", return_value=None) as mock_read,
        patch.dict(
            "esphome.__main__.POST_CONFIG_ACTIONS",
            {"upload": lambda args, config: 0},
        ),
    ):
        run_esphome(["esphome", "-s", "var", "val", "upload", str(fresh_cache_files)])

    mock_read.assert_called_once()


def test_run_esphome_compile_does_not_use_cache(fresh_cache_files: Path) -> None:
    """The compile subcommand always re-validates -- it's what writes the cache."""
    with (
        patch("esphome.__main__.read_config", return_value=None) as mock_read,
        patch.dict(
            "esphome.__main__.POST_CONFIG_ACTIONS",
            {"compile": lambda args, config: 0},
        ),
    ):
        run_esphome(["esphome", "compile", str(fresh_cache_files)])

    mock_read.assert_called_once()


def test_save_compiled_config_writes_cache(tmp_path: Path) -> None:
    """`save_compiled_config` writes the dumped YAML next to the sidecar."""
    CORE.config_path = tmp_path / "lite_test.yaml"
    save_compiled_config({"esphome": {"name": "lite_test"}, "logger": {}})

    cache_path = compiled_config_path("lite_test.yaml")
    assert cache_path.is_file()
    body = cache_path.read_text()
    assert "name: lite_test" in body
    assert "logger:" in body


def test_save_compiled_config_swallows_dump_errors(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Failures during the dump are non-fatal -- a bad cache just means
    the next fast path falls back to read_config()."""
    CORE.config_path = tmp_path / "lite_test.yaml"
    with patch("esphome.yaml_util.dump", side_effect=RuntimeError("boom")):
        save_compiled_config({"esphome": {"name": "lite_test"}})
    assert not compiled_config_path("lite_test.yaml").exists()


def test_load_compiled_config_rejects_wizard_only_sidecar(tmp_path: Path) -> None:
    """A wizard-only sidecar (no compile -- no core_platform / target_platform)
    can't drive upload/logs, so the fast path falls back."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")
    CORE.config_path = yaml_path

    storage_dir = tmp_path / ".esphome" / "storage"
    storage_dir.mkdir(parents=True, exist_ok=True)
    # StorageJSON with both core_platform and target_platform unset.
    (storage_dir / "lite_test.yaml.json").write_text(
        '{"storage_version": 1, "name": "lite_test", "friendly_name": null, '
        '"comment": null, "esphome_version": null, "src_version": 1, '
        '"address": null, "web_port": null, "esp_platform": null, '
        '"build_path": null, "firmware_bin_path": null, '
        '"loaded_integrations": [], "loaded_platforms": [], "no_mdns": false, '
        '"framework": null, "core_platform": null}'
    )
    cache_path = _write_cache(storage_dir / "lite_test.yaml.validated.yaml")
    _set_cache_mtime(cache_path, yaml_path, offset=5)

    assert load_compiled_config(yaml_path) is None
