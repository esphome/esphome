"""Tests for the Haier water heater configuration."""

import pytest

from esphome import config_validation as cv
from esphome.components import uart
from esphome.components.haier_water_heater.water_heater import (
    CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA,
)
from esphome.config import Config
from esphome.const import PlatformFramework
from esphome.core import ID
from tests.component_tests.types import SetCoreConfigCallable


def test_visible_by_default(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    config = CONFIG_SCHEMA({"name": "Heater capture"})
    assert config.get("internal", False) is False
    assert "protocol" not in config
    assert config["communication_timeout"].total_milliseconds == 20000


@pytest.mark.parametrize("protocol", ["HON", "SMARTAIR2", "unknown"])
def test_rejects_protocol_option(
    set_core_config: SetCoreConfigCallable, protocol: str
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({"name": "Heater capture", "protocol": protocol})


def test_rejects_climate_control_options(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(
            {"name": "Heater capture", "control_method": "SET_GROUP_PARAMETERS"}
        )


@pytest.mark.parametrize("missing", [None, "rx_pin", "tx_pin"])
@pytest.mark.parametrize(
    ("setting", "value"),
    [
        (None, None),
        ("baud_rate", 115200),
        ("data_bits", 7),
        ("parity", "EVEN"),
        ("stop_bits", 2),
    ],
)
def test_uart_requirements(
    set_core_config: SetCoreConfigCallable,
    missing: str | None,
    setting: str | None,
    value: int | str | None,
) -> None:
    bus_id = ID("heater_uart", is_declaration=True, type=uart.IDFUARTComponent)
    bus = {
        "id": bus_id,
        "baud_rate": 9600,
        "data_bits": 8,
        "parity": "NONE",
        "stop_bits": 1,
        "rx_pin": {"number": 16},
        "tx_pin": {"number": 17},
    }
    if missing is not None:
        del bus[missing]
    if setting is not None:
        bus[setting] = value
    full = Config()
    full["uart"] = [bus]
    full.declare_ids.append((bus_id, ["uart", 0, "id"]))
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full)
    config = {"uart_id": ID("heater_uart", type=uart.IDFUARTComponent)}
    if missing is None and setting is None:
        FINAL_VALIDATE_SCHEMA(config)
    else:
        with pytest.raises(cv.Invalid):
            FINAL_VALIDATE_SCHEMA(config)


def test_read_only_entities(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    config = CONFIG_SCHEMA(
        {
            "name": "Water Heater",
            "internal": False,
            "target_temperature": {"name": "Target"},
            "estimated_heating_time": {"name": "Estimated Heating Time"},
            "available_water": {"name": "Water Segments"},
            "communication": {"name": "Communication"},
            "power": {"name": "Power"},
            "heating": {"name": "Heating"},
            "mode": {"name": "Mode"},
        }
    )
    assert config["target_temperature"]["unit_of_measurement"] == "°C"
    assert config["estimated_heating_time"]["unit_of_measurement"] == "min"
    assert config["estimated_heating_time"]["device_class"] == "duration"
    assert config["communication"]["device_class"] == "connectivity"
    assert config["heating"]["device_class"] == "heat"
    assert config["internal"] is False


def test_custom_communication_timeout(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    config = CONFIG_SCHEMA({"name": "Heater", "communication_timeout": "7500ms"})
    assert config["communication_timeout"].total_milliseconds == 7500
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({"name": "Heater", "communication_timeout": "-1s"})
