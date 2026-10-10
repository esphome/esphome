"""Tests for the adafruit_ble OTA platform and its nrf52 integration."""

from collections.abc import Callable, Generator
from copy import deepcopy
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.adafruit_ble.ota import _final_validate
from esphome.components.nrf52 import (
    ADAFRUIT_BLE_OTA_PLATFORM,
    _final_validate as nrf52_final_validate,
)
from esphome.components.nrf52.const import BOOTLOADER_ADAFRUIT_NRF52_SD140_V6
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOOTLOADER,
    KEY_ZEPHYR,
)
from esphome.config import Config
from esphome.const import (
    CONF_ADVANCED,
    CONF_DISABLED,
    CONF_ENABLE_OTA_ROLLBACK,
    CONF_FRAMEWORK,
    CONF_OTA,
    CONF_PLATFORM,
    CONF_SAFE_MODE,
    Toolchain,
)
from esphome.core import CORE
import esphome.final_validate as fv

NRF52_CONFIG = {
    KEY_BOOTLOADER: BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    CONF_FRAMEWORK: {CONF_ADVANCED: {CONF_ENABLE_OTA_ROLLBACK: True}},
}


@pytest.fixture
def adafruit_ble_ota_full_config() -> Generator[None]:
    """Full config as final validation sees it with adafruit_ble OTA configured."""
    full = Config()
    full[CONF_OTA] = [{CONF_PLATFORM: ADAFRUIT_BLE_OTA_PLATFORM}]
    full[CONF_SAFE_MODE] = {CONF_DISABLED: False}
    token = fv.full_config.set(full)
    yield
    fv.full_config.reset(token)


def test_adafruit_ble_config_builds_ota_component(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("adafruit_ble.yaml"))
    assert "adafruit_ble::OTAComponent" in main_cpp


def test_requires_zephyr_ble_server() -> None:
    CORE.loaded_integrations = set()
    CORE.data[KEY_ZEPHYR] = {KEY_BOOTLOADER: BOOTLOADER_ADAFRUIT_NRF52_SD140_V6}
    with pytest.raises(cv.Invalid, match="zephyr_ble_server"):
        _final_validate({})


def test_requires_adafruit_bootloader() -> None:
    CORE.loaded_integrations = {"zephyr_ble_server"}
    CORE.data[KEY_ZEPHYR] = {KEY_BOOTLOADER: BOOTLOADER_MCUBOOT}
    with pytest.raises(cv.Invalid, match="requires an Adafruit bootloader"):
        _final_validate({})


def test_rollback_rejected_when_enabled_by_user(
    adafruit_ble_ota_full_config: None,
) -> None:
    CORE.toolchain = Toolchain.SDK_NRF
    CORE.raw_config = {
        "nrf52": {CONF_FRAMEWORK: {CONF_ADVANCED: {CONF_ENABLE_OTA_ROLLBACK: True}}}
    }
    with pytest.raises(cv.Invalid, match=CONF_ENABLE_OTA_ROLLBACK):
        nrf52_final_validate(deepcopy(NRF52_CONFIG))


def test_rollback_disabled_by_default(adafruit_ble_ota_full_config: None) -> None:
    CORE.toolchain = Toolchain.SDK_NRF
    CORE.raw_config = {}
    config = deepcopy(NRF52_CONFIG)
    nrf52_final_validate(config)
    assert config[CONF_FRAMEWORK][CONF_ADVANCED][CONF_ENABLE_OTA_ROLLBACK] is False
