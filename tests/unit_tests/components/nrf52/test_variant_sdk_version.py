"""The SoC variant taken from the board, and the oldest sdk-nrf version it supports."""

from collections.abc import Iterator
from unittest.mock import PropertyMock, patch

import pytest

from esphome.components import nrf52
from esphome.components.nrf52.const import (
    VARIANT_NRF54L15,
    VARIANT_NRF54LM20,
    VARIANT_NRF51822,
    VARIANT_NRF52840,
)
from esphome.components.nrf52.framework import get_nrf52_variant, set_nrf52_variant
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
)
from esphome.core import CORE, EsphomeCore
from esphome.types import ConfigType


@pytest.mark.parametrize(
    ("board", "variant"),
    [
        ("nrf54lm20dk/nrf54lm20a/cpuapp", VARIANT_NRF54LM20),
        ("NRF54LM20DK/NRF54LM20A/CPUAPP", VARIANT_NRF54LM20),
        ("xiao_ble", VARIANT_NRF52840),
        ("bbc_microbit", VARIANT_NRF51822),
        ("adafruit_feather_nrf52840", VARIANT_NRF52840),
        ("nrf54lm20dk", VARIANT_NRF54LM20),
        ("nrf54l15dk/nrf54l15/cpuapp", VARIANT_NRF54L15),
        ("nrf54l15dk", VARIANT_NRF54L15),
        ("nrf54lm20dk/nrf54lm20b/cpuapp", VARIANT_NRF54LM20),
    ],
)
def test_board_variant(board: str, variant: str) -> None:
    assert nrf52._board_variant(board) == variant


def test_variant_defaults_to_nrf52840() -> None:
    assert get_nrf52_variant() == VARIANT_NRF52840


@pytest.fixture
def platformio() -> Iterator[PropertyMock]:
    with patch.object(
        EsphomeCore,
        "using_toolchain_platformio",
        new_callable=PropertyMock,
        return_value=False,
    ) as mock:
        CORE.data.setdefault(KEY_CORE, {})
        yield mock


def _set_framework(version: str | None) -> ConfigType:
    framework = {} if version is None else {CONF_VERSION: version}
    return nrf52.set_framework({CONF_FRAMEWORK: framework})


@pytest.mark.usefixtures("platformio")
@pytest.mark.parametrize(
    ("variant", "default"),
    [
        (VARIANT_NRF54LM20, "3.4.1"),
        (VARIANT_NRF52840, nrf52.RECOMMENDED_SDK_NRF_VERSION),
    ],
)
def test_default_sdk_version(variant: str, default: str) -> None:
    set_nrf52_variant(variant)
    config = _set_framework(None)
    assert config[CONF_FRAMEWORK][CONF_VERSION] == default
    assert CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] == cv.Version.parse(default)


@pytest.mark.usefixtures("platformio")
@pytest.mark.parametrize("version", ["3.4.1", "3.5.0"])
def test_nrf54lm20_accepts_minimum_or_newer(version: str) -> None:
    set_nrf52_variant(VARIANT_NRF54LM20)
    assert _set_framework(version)[CONF_FRAMEWORK][CONF_VERSION] == version


@pytest.mark.usefixtures("platformio")
@pytest.mark.parametrize("version", ["2.9.2", "3.4.0"])
def test_nrf54lm20_rejects_older_sdk(version: str) -> None:
    set_nrf52_variant(VARIANT_NRF54LM20)
    with pytest.raises(cv.Invalid, match="needs sdk-nrf 3.4.1 or newer"):
        _set_framework(version)


def test_nrf54lm20_rejects_platformio(platformio: PropertyMock) -> None:
    platformio.return_value = True
    set_nrf52_variant(VARIANT_NRF54LM20)
    with pytest.raises(cv.Invalid, match="is not supported by PlatformIO"):
        _set_framework(None)


@pytest.mark.parametrize(
    ("board", "resolved"),
    [
        ("nrf54lm20dk", "nrf54lm20dk/nrf54lm20a/cpuapp"),
        ("nrf54l15dk", "nrf54l15dk/nrf54l15/cpuapp"),
        ("nrf54lm20dk/nrf54lm20b/cpuapp", "nrf54lm20dk/nrf54lm20b/cpuapp"),
        ("xiao_ble", "xiao_ble"),
    ],
)
def test_resolve_board_alias(board: str, resolved: str) -> None:
    config = nrf52._resolve_board_alias({CONF_BOARD: board})
    assert config[CONF_BOARD] == resolved
