"""Reject unsafe zone writes and unsupported transport settings."""

import pytest

from esphome.components.ld6004 import CONFIG_SCHEMA, SET_ZONE_SCHEMA
import esphome.config_validation as cv
from tests.component_tests.types import SetCoreConfigCallable


def test_minimal() -> None:
    assert CONFIG_SCHEMA({"id": "radar"})["stale_timeout"].total_milliseconds == 5000


@pytest.mark.parametrize("value", [float("inf"), float("nan"), -1e40, 7])
def test_zone_bounds(value: float) -> None:
    zone: dict = {
        "zone_id": 4,
        "x_min": value,
        "x_max": 5,
        "y_min": -1,
        "y_max": 1,
        "z_min": -1,
        "z_max": 1,
    }
    with pytest.raises(cv.Invalid):
        SET_ZONE_SCHEMA(zone)


def test_inverted_zone() -> None:
    with pytest.raises(cv.Invalid):
        SET_ZONE_SCHEMA(
            {
                "zone_id": 12,
                "x_min": 2,
                "x_max": 1,
                "y_min": -1,
                "y_max": 1,
                "z_min": -1,
                "z_max": 1,
            }
        )


@pytest.mark.parametrize("bad_id", [-1, 12])
def test_zone_id(bad_id: int) -> None:
    with pytest.raises(cv.Invalid):
        SET_ZONE_SCHEMA(
            {
                "zone_id": bad_id,
                "x_min": -1,
                "x_max": 1,
                "y_min": -1,
                "y_max": 1,
                "z_min": -1,
                "z_max": 1,
            }
        )


@pytest.mark.parametrize("axis", ["x", "y", "z"])
def test_zone_order(axis: str) -> None:
    zone: dict = {
        "zone_id": 4,
        "x_min": -1,
        "x_max": 1,
        "y_min": -1,
        "y_max": 1,
        "z_min": -1,
        "z_max": 1,
    }
    zone[f"{axis}_max"] = -1
    with pytest.raises(cv.Invalid, match="less than"):
        SET_ZONE_SCHEMA(zone)


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


def test_templated_zone() -> None:
    from esphome.core import Lambda

    SET_ZONE_SCHEMA(
        {
            "zone_id": Lambda("return 4;"),
            **{
                axis: Lambda("return 1.0f;")
                for axis in ("x_min", "x_max", "y_min", "y_max", "z_min", "z_max")
            },
        }
    )
