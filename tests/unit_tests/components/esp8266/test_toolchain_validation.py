"""Tests for the native (non-PlatformIO) toolchain config validation."""

from __future__ import annotations

from collections.abc import Generator

import pytest

from esphome.components.esp8266 import (
    ARDUINO_4_PLATFORM_VERSION,
    _format_framework_arduino_version,
    _parse_platform_version,
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
def _arduino_toolchain() -> Generator[None]:
    CORE.toolchain = Toolchain.ARDUINO
    yield
    CORE.toolchain = None


def _config(
    version: str = "3.1.2",
    source: str | None = None,
    platform_version: str | None = None,
    board: str = "nodemcuv2",
) -> ConfigType:
    return {
        CONF_FRAMEWORK: {
            CONF_VERSION: version,
            CONF_SOURCE: source
            or _format_framework_arduino_version(cv.Version.parse(version)),
            CONF_PLATFORM_VERSION: platform_version
            or _parse_platform_version(str(ARDUINO_4_PLATFORM_VERSION)),
        },
        CONF_BOARD: board,
    }


def test_valid_config_passes() -> None:
    config = _config()
    assert _validate_native_toolchain(config) is config


def test_platformio_toolchain_skips_checks() -> None:
    CORE.toolchain = Toolchain.PLATFORMIO
    config = _config(version="2.7.4", board="not_a_board")
    assert _validate_native_toolchain(config) is config


def test_version_floor_is_3_1_1() -> None:
    # 3.1.0 has no registry package, so the native floor is 3.1.1
    with pytest.raises(cv.Invalid, match="3.1.1 or newer"):
        _validate_native_toolchain(_config(version="3.1.0"))
    _validate_native_toolchain(_config(version="3.1.1"))


def test_custom_platform_version_warns(caplog: pytest.LogCaptureFixture) -> None:
    _validate_native_toolchain(
        _config(platform_version="platformio/espressif8266@4.0.1")
    )
    assert "'platform_version' is ignored" in caplog.text


def test_default_platform_version_does_not_warn(
    caplog: pytest.LogCaptureFixture,
) -> None:
    _validate_native_toolchain(_config())
    assert "'platform_version' is ignored" not in caplog.text


def test_custom_source_rejected() -> None:
    with pytest.raises(cv.Invalid, match="custom framework source"):
        _validate_native_toolchain(
            _config(source="https://github.com/esp8266/Arduino.git")
        )


def test_unsupported_board_rejected() -> None:
    with pytest.raises(cv.Invalid, match="not supported by"):
        _validate_native_toolchain(_config(board="not_a_board"))
