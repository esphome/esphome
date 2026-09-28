"""Reject unsupported transport settings and missing OUT pins."""

import pytest

from esphome.components.ld6004 import CONFIG_SCHEMA
import esphome.config_validation as cv
from tests.component_tests.types import SetCoreConfigCallable


def test_minimal() -> None:
    assert CONFIG_SCHEMA({"id": "radar"})["stale_timeout"].total_milliseconds == 5000


def test_out_needs_same_instance_pin(set_core_config: SetCoreConfigCallable) -> None:
    from esphome.components.ld6004.binary_sensor import final_validate
    from esphome.config import Config
    from esphome.const import PlatformFramework
    from esphome.core import ID

    full: Config = Config()
    hub_id: ID = ID("radar", is_declaration=True)
    full["ld6004"] = [
        {"id": hub_id},
        {"id": ID("other", is_declaration=True), "out_pin": {"number": 2}},
    ]
    full.declare_ids.append((hub_id, ["ld6004", 0, "id"]))
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full)
    with pytest.raises(cv.Invalid, match="same ld6004"):
        final_validate({"ld6004_id": hub_id, "out": {"name": "OUT"}})
    full["ld6004"][0]["out_pin"] = {"number": 2}
    final_validate({"ld6004_id": hub_id, "out": {"name": "OUT"}})


@pytest.mark.parametrize(
    "key,value",
    [("baud_rate", 9600), ("data_bits", 7), ("parity", "EVEN"), ("stop_bits", 2)],
)
def test_uart_format(
    set_core_config: SetCoreConfigCallable, key: str, value: object
) -> None:
    from esphome.components.ld6004 import FINAL_VALIDATE_SCHEMA
    from esphome.config import Config
    from esphome.const import PlatformFramework
    from esphome.core import ID

    full: Config = Config()
    uart_id: ID = ID("bus", is_declaration=True)
    uart: dict = {
        "id": uart_id,
        "baud_rate": 115200,
        "data_bits": 8,
        "parity": "NONE",
        "stop_bits": 1,
        "rx_pin": {},
        "tx_pin": {},
    }
    uart[key] = value
    full["uart"] = [uart]
    full.declare_ids.append((uart_id, ["uart", 0, "id"]))
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full)
    with pytest.raises(cv.Invalid):
        FINAL_VALIDATE_SCHEMA({"uart_id": uart_id})


@pytest.mark.parametrize("key", ["zones", "on_point_cloud"])
def test_removed_hub_options(key: str) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({"id": "radar", key: []})
