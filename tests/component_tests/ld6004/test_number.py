"""Validate LD6004 number metadata and area configuration keys."""

import pytest

from esphome.components.ld600x.entities import LD600XNumber
from esphome.components.ld6004 import LD6004Component
from esphome.components.ld6004.number import CONFIG_SCHEMA
from esphome.const import PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


@pytest.mark.parametrize(
    "keys",
    [
        (),
        ("dwell_lifetime",),
        ("output_interval",),
        ("dwell_lifetime", "output_interval"),
    ],
)
def test_model_numbers(
    set_core_config: SetCoreConfigCallable, keys: tuple[str, ...]
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    config: ConfigType = CONFIG_SCHEMA(
        {"ld6004_id": "radar", **{key: {"name": key} for key in keys}}
    )
    assert config["ld6004_id"].type == LD6004Component
    assert str(config["ld6004_id"]) == "radar"
    assert set(config) == {"ld6004_id", *keys}
    for key in keys:
        assert config[key]["id"].type == LD600XNumber
        assert config[key]["entity_category"] == "config"
        assert "unit_of_measurement" not in config[key]


def test_area_config_keys(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    keys: tuple[str, ...] = ("x_min", "x_max", "y_min", "y_max", "z_min", "z_max")
    config: ConfigType = CONFIG_SCHEMA(
        {"area_config": {key: {"name": key} for key in keys}}
    )
    assert config["ld6004_id"].type == LD6004Component
    assert set(config["area_config"]) == set(keys)
    for key in keys:
        assert config["area_config"][key]["id"].type == LD600XNumber
        assert config["area_config"][key]["unit_of_measurement"] == "m"
        assert config["area_config"][key]["entity_category"] == "config"
