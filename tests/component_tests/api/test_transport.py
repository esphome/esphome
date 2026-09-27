"""Tests for the api transport option."""

import pytest

from esphome.components.api import (
    AUTO_LOAD,
    CONF_TRANSPORT,
    DEFAULT_PORT,
    TRANSPORT_BLE,
    TRANSPORT_IP,
    _validate_ble_transport,
    _validate_transport,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENCRYPTION,
    CONF_KEY,
    CONF_PORT,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
)
from esphome.core import CORE


def test_auto_load_probe_returns_every_transport() -> None:
    loads = AUTO_LOAD(None)
    assert {"socket", "network", "socket_ble"} <= set(loads)


def test_auto_load_ip_transport() -> None:
    loads = AUTO_LOAD({CONF_TRANSPORT: TRANSPORT_IP})
    assert "socket" in loads
    assert "socket_ble" not in loads


def test_auto_load_ble_transport() -> None:
    loads = AUTO_LOAD({CONF_TRANSPORT: TRANSPORT_BLE})
    assert "socket_ble" in loads
    assert "socket" not in loads
    assert "network" not in loads


def test_ble_transport_accepted_on_nrf52() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "nrf52"}
    assert _validate_transport(TRANSPORT_BLE) == TRANSPORT_BLE


@pytest.mark.parametrize("platform", ["esp32", "esp8266", "rp2"])
def test_ble_transport_rejected_elsewhere(platform: str) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: platform}
    with pytest.raises(cv.Invalid, match="only available on"):
        _validate_transport(TRANSPORT_BLE)


def test_ip_transport_accepted_everywhere() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp8266"}
    assert _validate_transport(TRANSPORT_IP) == TRANSPORT_IP


_ENCRYPTION = {CONF_KEY: "bOFFzzvfpg5DB94DuBGLXD/hMnhpDKgP9UQyBulwWVU="}


def test_ble_transport_rejects_a_port() -> None:
    config = {
        CONF_TRANSPORT: TRANSPORT_BLE,
        CONF_PORT: 8000,
        CONF_ENCRYPTION: _ENCRYPTION,
    }
    with pytest.raises(cv.Invalid, match="does not support setting a port"):
        _validate_ble_transport(config)


def test_ble_transport_accepts_the_default_port_with_a_key() -> None:
    config = {
        CONF_TRANSPORT: TRANSPORT_BLE,
        CONF_PORT: DEFAULT_PORT,
        CONF_ENCRYPTION: _ENCRYPTION,
    }
    assert _validate_ble_transport(config) == config


@pytest.mark.parametrize("extra", [{}, {CONF_ENCRYPTION: {}}])
def test_ble_transport_requires_an_encryption_key(extra: dict[str, dict]) -> None:
    config = {CONF_TRANSPORT: TRANSPORT_BLE, CONF_PORT: DEFAULT_PORT} | extra
    with pytest.raises(cv.Invalid, match="requires an encryption key"):
        _validate_ble_transport(config)


def test_ip_transport_needs_no_encryption() -> None:
    config = {CONF_TRANSPORT: TRANSPORT_IP, CONF_PORT: 8000}
    assert _validate_ble_transport(config) == config
