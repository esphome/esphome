"""Tests for the native (non-PlatformIO) toolchain config validation."""

from __future__ import annotations

import pytest

from esphome.components.esp8266 import (
    ARDUINO_FRAMEWORK_SCHEMA,
    _validate_native_toolchain,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_PLATFORM_VERSION,
    CONF_SOURCE,
    CONF_VERSION,
    Toolchain,
)
from esphome.core import CORE
from esphome.types import ConfigType


@pytest.fixture(autouse=True)
def _arduino_toolchain() -> None:
    # The suite-wide reset_core fixture clears CORE.toolchain after each test
    CORE.toolchain = Toolchain.ARDUINO


def _config(board: str = "nodemcuv2", **framework: str) -> ConfigType:
    framework.setdefault(CONF_VERSION, "3.1.2")
    # The real schema fills the source/platform_version defaults, so these
    # tests validate against what config validation actually emits
    return {
        CONF_FRAMEWORK: ARDUINO_FRAMEWORK_SCHEMA(framework),
        CONF_BOARD: board,
    }


def test_valid_config_passes() -> None:
    config = _config()
    assert _validate_native_toolchain(config) is config


def test_platformio_toolchain_skips_checks() -> None:
    CORE.toolchain = Toolchain.PLATFORMIO
    config = _config(board="not_a_board", **{CONF_VERSION: "2.7.4"})
    assert _validate_native_toolchain(config) is config


def test_version_below_floor_rejected() -> None:
    # 3.1.0 has no registry package, so the native floor is 3.1.1
    with pytest.raises(cv.Invalid, match="3.1.1 or newer"):
        _validate_native_toolchain(_config(**{CONF_VERSION: "3.1.0"}))


def test_version_at_floor_accepted() -> None:
    _validate_native_toolchain(_config(**{CONF_VERSION: "3.1.1"}))


def test_custom_platform_version_warns_and_is_dropped(
    caplog: pytest.LogCaptureFixture,
) -> None:
    config = _config(**{CONF_PLATFORM_VERSION: "platformio/espressif8266@4.0.1"})
    _validate_native_toolchain(config)
    assert "'platform_version' is ignored" in caplog.text
    assert CONF_PLATFORM_VERSION not in config[CONF_FRAMEWORK]


def test_default_platform_version_does_not_warn(
    caplog: pytest.LogCaptureFixture,
) -> None:
    config = _config()
    _validate_native_toolchain(config)
    assert "'platform_version' is ignored" not in caplog.text
    assert CONF_PLATFORM_VERSION not in config[CONF_FRAMEWORK]


def test_custom_source_rejected() -> None:
    with pytest.raises(cv.Invalid, match="custom framework source"):
        _validate_native_toolchain(
            _config(**{CONF_SOURCE: "https://github.com/esp8266/Arduino.git"})
        )


def test_unsupported_board_rejected() -> None:
    with pytest.raises(cv.Invalid, match="not supported by"):
        _validate_native_toolchain(_config(board="not_a_board"))
