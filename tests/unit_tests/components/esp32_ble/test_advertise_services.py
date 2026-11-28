"""Tests for the esp32_ble advertise_services final validation."""

from __future__ import annotations

from unittest.mock import patch

import pytest

from esphome import config_validation as cv, final_validate as fv
from esphome.components import esp32_ble


def _validate(config: dict, full_config: dict) -> None:
    token = fv.full_config.set(full_config)
    try:
        with (
            patch.object(esp32_ble, "validate_variant"),
            patch.object(esp32_ble, "get_esp32_variant", return_value="ESP32"),
            patch.object(esp32_ble, "add_idf_sdkconfig_option"),
            patch.object(esp32_ble, "validate_connection_slots"),
        ):
            esp32_ble.final_validation(config)
    finally:
        fv.full_config.reset(token)


def test_default_needs_no_beacon() -> None:
    _validate({"advertise_services": True}, {"esp32_ble": {}})


def test_disabled_without_beacon_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="requires 'esp32_ble_beacon'"):
        _validate({"advertise_services": False}, {"esp32_ble": {}})


def test_disabled_with_beacon_is_accepted() -> None:
    _validate({"advertise_services": False}, {"esp32_ble": {}, "esp32_ble_beacon": {}})


def test_disabled_with_server_warns(caplog: pytest.LogCaptureFixture) -> None:
    _validate(
        {"advertise_services": False},
        {"esp32_ble": {}, "esp32_ble_beacon": {}, "esp32_ble_server": {}},
    )
    assert "GATT server" in caplog.text
