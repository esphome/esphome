"""Tests for the native (non-PlatformIO) toolchain config validation."""

from __future__ import annotations

from collections.abc import Generator
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import pytest

from esphome.components import esp8266
from esphome.components.esp8266 import (
    ARDUINO_FRAMEWORK_SCHEMA,
    _resolve_toolchain,
    _validate_native_toolchain,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_PLATFORM_VERSION,
    CONF_SOURCE,
    CONF_TOOLCHAIN,
    CONF_VERSION,
    Toolchain,
)
from esphome.core import CORE, EsphomeError
from esphome.types import ConfigType


@pytest.fixture(autouse=True)
def _arduino_toolchain() -> Generator[None]:
    # The suite-wide reset_core fixture clears CORE.toolchain after each test
    CORE.toolchain = Toolchain.ARDUINO
    esp8266._DECODE_WARNED_AT.clear()
    yield
    esp8266._DECODE_WARNED_AT.clear()


def _config(
    board: str = "nodemcuv2",
    version: str = "3.1.2",
    source: str | None = None,
    platform_version: str | None = None,
) -> ConfigType:
    framework: dict[str, str] = {CONF_VERSION: version}
    if source is not None:
        framework[CONF_SOURCE] = source
    if platform_version is not None:
        framework[CONF_PLATFORM_VERSION] = platform_version
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
    config = _config(board="not_a_board", version="2.7.4")
    assert _validate_native_toolchain(config) is config


def test_version_below_floor_rejected() -> None:
    # 3.1.0 has no registry package, so the native floor is 3.1.1
    with pytest.raises(cv.Invalid, match="3.1.1 or newer"):
        _validate_native_toolchain(_config(version="3.1.0"))


def test_version_at_floor_accepted() -> None:
    _validate_native_toolchain(_config(version="3.1.1"))


def test_custom_platform_version_warns_and_is_dropped(
    caplog: pytest.LogCaptureFixture,
) -> None:
    config = _config(platform_version="platformio/espressif8266@4.0.1")
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
            _config(source="https://github.com/esp8266/Arduino.git")
        )


def test_unsupported_board_rejected() -> None:
    with pytest.raises(cv.Invalid, match="not supported by"):
        _validate_native_toolchain(_config(board="not_a_board"))


def test_yaml_toolchain_key_resolves() -> None:
    """The documented `toolchain: arduino` YAML key selects the native path."""
    CORE.toolchain = None
    _resolve_toolchain({CONF_TOOLCHAIN: Toolchain.ARDUINO})
    assert CORE.toolchain == Toolchain.ARDUINO
    assert CORE.using_toolchain_arduino


def test_yaml_toolchain_key_defaults_to_platformio() -> None:
    CORE.toolchain = None
    _resolve_toolchain({})
    assert CORE.toolchain == Toolchain.PLATFORMIO


def test_decode_pc_native_missing_tools_warns_once(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A stack dump of many addresses produces one missing-tool warning."""

    with (
        patch(
            "esphome.arduino8266.toolchain.get_addr2line_path",
            return_value=tmp_path / "missing-addr2line",
        ),
        patch(
            "esphome.arduino8266.toolchain.get_elf_path",
            return_value=tmp_path / "missing.elf",
        ),
    ):
        esp8266._decode_pc({}, "40201234")
        esp8266._decode_pc({}, "40201238")
    assert caplog.text.count("Cannot decode crash addresses") == 1


def test_decode_pc_platformio_missing_tools_warns_once(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The PlatformIO branch reports a missing addr2line/ELF at the same
    warning level as the native one; raw undecoded addresses with no
    stated reason are undiagnosable at default log level."""

    CORE.toolchain = Toolchain.PLATFORMIO
    idedata = SimpleNamespace(addr2line_path=None, firmware_elf_path=None)
    with patch("esphome.platformio.toolchain.get_idedata", return_value=idedata):
        esp8266._decode_pc({}, "40201234")
        esp8266._decode_pc({}, "40201238")
    assert caplog.text.count("Cannot decode crash addresses") == 1


def test_resolve_toolchain_rejects_unsupported() -> None:
    """ESP8266 rejects a CLI toolchain it cannot serve, like every platform."""

    CORE.toolchain = Toolchain.SDK_NRF
    with pytest.raises(cv.Invalid, match="Unsupported toolchain 'sdk-nrf'"):
        _resolve_toolchain({})


def test_run_compile_platformio_falls_through() -> None:
    """Under toolchain: platformio the hook returns False without touching
    the native backend; this is what keeps existing users on PlatformIO."""
    CORE.toolchain = Toolchain.PLATFORMIO
    with patch("esphome.arduino8266.toolchain.run_compile") as mock_native:
        assert esp8266.run_compile(SimpleNamespace(), {}) is False
    mock_native.assert_not_called()


def test_run_compile_arduino_failure_raises() -> None:
    """A non-zero native build fails by name instead of returning success."""
    CORE.verbose = False
    with (
        patch("esphome.arduino8266.toolchain.run_compile", return_value=1),
        pytest.raises(EsphomeError, match="native build failed"),
    ):
        esp8266.run_compile(SimpleNamespace(), {})


def test_copy_files_native_skips_platformio_scripts(tmp_path: Path) -> None:
    """The native build writes no PlatformIO extra scripts."""
    CORE.build_path = tmp_path
    esp8266.copy_files()
    assert list(tmp_path.iterdir()) == []
