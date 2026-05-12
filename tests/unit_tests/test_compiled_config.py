"""Tests for the validated-config cache used by upload/logs."""

from __future__ import annotations

import os
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.__main__ import run_esphome
from esphome.compiled_config import compiled_config_path, load_compiled_config
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
  build_path: build/lite_test
esp32:
  board: nodemcu-32s
  framework:
    type: arduino
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
    """YAML + cache, both consistent and fresh."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")
    CORE.config_path = yaml_path

    cache = _write_cache(
        tmp_path / ".esphome" / "storage" / "lite_test.yaml.validated.yaml"
    )
    _set_cache_mtime(cache, yaml_path, offset=5)

    return yaml_path


def test_compiled_config_path_lives_alongside_sidecar(setup_core: Path) -> None:
    """The cache file shape is predictable from the YAML filename."""
    assert str(compiled_config_path("device.yaml")).endswith(
        "storage/device.yaml.validated.yaml"
    )


def test_load_compiled_config_happy_path(fresh_cache_files: Path) -> None:
    """Fresh cache → returns the config and populates CORE from it."""
    config = load_compiled_config(fresh_cache_files)

    assert config is not None
    assert config[CONF_ESPHOME][CONF_NAME] == "lite_test"
    assert config[CONF_API]["encryption"]["key"] == "6dGhpcyBpcyBhIHRlc3Q="
    assert config["ota"][0]["password"] == "secret"

    # CORE state derives from the cached config dict -- same source
    # `read_config` uses, no separate sidecar schema.
    assert CORE.name == "lite_test"
    assert CORE.friendly_name == "Lite Test Device"
    assert CORE.build_path == CORE.data_dir / "build/lite_test"
    assert CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] == "esp32"
    assert CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] == "arduino"


@pytest.mark.parametrize(
    "scenario",
    ["missing_cache", "stale_cache", "corrupt_cache", "missing_esphome_block"],
)
def test_load_compiled_config_falls_back(tmp_path: Path, scenario: str) -> None:
    """All non-happy cases return None so the caller falls back."""
    yaml_path = tmp_path / "lite_test.yaml"
    yaml_path.write_text("esphome:\n  name: lite_test\n")
    CORE.config_path = yaml_path
    cache_path = tmp_path / ".esphome" / "storage" / "lite_test.yaml.validated.yaml"

    if scenario == "missing_cache":
        pass  # no cache
    elif scenario == "stale_cache":
        _set_cache_mtime(_write_cache(cache_path), yaml_path, offset=-60)
    elif scenario == "corrupt_cache":
        _set_cache_mtime(
            _write_cache(cache_path, "not: valid: yaml: ["), yaml_path, offset=5
        )
    elif scenario == "missing_esphome_block":
        # Parseable YAML but no esphome: block -- can't populate CORE.
        _set_cache_mtime(_write_cache(cache_path, "logger:\n"), yaml_path, offset=5)

    assert load_compiled_config(yaml_path) is None


@pytest.mark.parametrize("command", ["upload", "logs"])
def test_run_esphome_upload_and_logs_use_cache_when_fresh(
    command: str, fresh_cache_files: Path
) -> None:
    """upload/logs skip read_config() when the cache is fresh."""
    captured: dict = {}

    def _stub(_args, config):
        captured["config"] = config
        return 0

    with (
        patch("esphome.__main__.read_config") as mock_read,
        patch.dict("esphome.__main__.POST_CONFIG_ACTIONS", {command: _stub}),
    ):
        assert run_esphome(["esphome", command, str(fresh_cache_files)]) == 0

    mock_read.assert_not_called()
    assert captured["config"][CONF_ESPHOME][CONF_NAME] == "lite_test"
    assert captured["config"][CONF_API]["encryption"]["key"] == "6dGhpcyBpcyBhIHRlc3Q="


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


def test_run_esphome_compile_does_not_use_cache(fresh_cache_files: Path) -> None:
    """compile always re-validates -- it's what writes the cache."""
    with (
        patch("esphome.__main__.read_config", return_value=None) as mock_read,
        patch.dict(
            "esphome.__main__.POST_CONFIG_ACTIONS",
            {"compile": lambda args, config: 0},
        ),
    ):
        run_esphome(["esphome", "compile", str(fresh_cache_files)])

    mock_read.assert_called_once()
