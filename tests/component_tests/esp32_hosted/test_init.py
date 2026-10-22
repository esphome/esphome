"""Tests for the esp32_hosted final validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_IDF_VERSION
from esphome.components.esp32_hosted import CONF_ACTIVE_HIGH, _final_validate
from esphome.const import PlatformFramework

from ..types import SetCoreConfigCallable


@pytest.mark.parametrize("idf", ["5.0.0", "5.2.2", "5.3.2", "5.4.2"])
def test_final_validate_rejects_old_idf(
    set_core_config: SetCoreConfigCallable, idf: str
) -> None:
    """ESP-IDF older than 5.5 is rejected with a clear error."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse(idf)},
    )
    with pytest.raises(cv.Invalid, match="requires ESP-IDF 5.5 or newer"):
        _final_validate({CONF_ACTIVE_HIGH: True})


def test_final_validate_rejects_active_low_reset(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """esp_hosted drives the reset line with a low pulse only."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse("5.5.6")},
    )
    with pytest.raises(cv.Invalid, match="active_high: false"):
        _final_validate({CONF_ACTIVE_HIGH: False})


@pytest.mark.parametrize("idf", ["5.5.0", "5.5.6"])
def test_final_validate_accepts_supported_idf(
    set_core_config: SetCoreConfigCallable, idf: str
) -> None:
    """ESP-IDF 5.5 and newer passes validation unchanged."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse(idf)},
    )
    _final_validate({CONF_ACTIVE_HIGH: True})
