"""Tests for the esp32_hosted validation."""

import logging

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_IDF_VERSION
from esphome.components.esp32_hosted import (
    CONF_ACTIVE_HIGH,
    _final_validate,
    _validate_active_high,
)
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
        _final_validate({})


@pytest.mark.parametrize("idf", ["5.5.0", "5.5.6"])
def test_final_validate_accepts_supported_idf(
    set_core_config: SetCoreConfigCallable, idf: str
) -> None:
    """ESP-IDF 5.5 and newer passes validation unchanged."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse(idf)},
    )
    _final_validate({})


def test_active_high_absent_is_silent(caplog: pytest.LogCaptureFixture) -> None:
    """A config without the deprecated key logs nothing."""
    with caplog.at_level(logging.WARNING):
        assert _validate_active_high({}) == {}
    assert "active_high" not in caplog.text


def test_active_high_true_warns(caplog: pytest.LogCaptureFixture) -> None:
    """The old required value still validates, with a deprecation warning."""
    with caplog.at_level(logging.WARNING):
        _validate_active_high({CONF_ACTIVE_HIGH: True})
    assert "'active_high' is deprecated" in caplog.text


def test_active_high_false_rejected() -> None:
    """esp_hosted drives the reset line with a low pulse only."""
    with pytest.raises(cv.Invalid, match="active_high: false"):
        _validate_active_high({CONF_ACTIVE_HIGH: False})
