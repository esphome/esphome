"""Tests for the genibus_ble final validation: one bus per BLE client."""

import pytest

from esphome import config_validation as cv
from esphome.components import genibus_ble
from esphome.components.ble_client import CONF_BLE_CLIENT_ID
from esphome.const import PlatformFramework
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _entry(client_id: str) -> ConfigType:
    return {CONF_BLE_CLIENT_ID: ID(client_id)}


def test_accepts_buses_on_distinct_clients(
    set_core_config: SetCoreConfigCallable,
) -> None:
    entries = [_entry("pump_a"), _entry("pump_b")]
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config={genibus_ble.DOMAIN: entries}
    )
    for entry in entries:
        genibus_ble._final_validate(entry)


def test_rejects_two_buses_on_one_client(
    set_core_config: SetCoreConfigCallable,
) -> None:
    entries = [_entry("pump"), _entry("pump")]
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config={genibus_ble.DOMAIN: entries}
    )
    with pytest.raises(cv.Invalid, match="used by more than one genibus_ble"):
        genibus_ble._final_validate(entries[0])
