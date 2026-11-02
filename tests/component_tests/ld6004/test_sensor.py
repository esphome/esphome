"""Validate LD6004 sensor target and area limits."""

import pytest

from esphome.components.ld6004 import LD6004Component
from esphome.components.ld6004.sensor import CONFIG_SCHEMA
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


@pytest.mark.parametrize(
    ("key", "fields"),
    [
        ("target_3", ("x", "y", "z", "doppler_index", "cluster_id")),
        ("dwell_area_3", ("x_min", "x_max", "y_min", "y_max", "z_min", "z_max")),
    ],
)
def test_model_sensor_keys(
    set_core_config: SetCoreConfigCallable, key: str, fields: tuple[str, ...]
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    config: ConfigType = CONFIG_SCHEMA(
        {key: {field: {"name": field} for field in fields}}
    )
    assert config["ld6004_id"].type == LD6004Component
    assert set(config[key]) == set(fields)
    for field in fields:
        assert config[key][field]["name"] == field


@pytest.mark.parametrize(
    "key",
    [
        "target_0",
        "target_4",
        "interference_area_4",
        "detection_area_4",
        "dwell_area_4",
    ],
)
def test_out_of_range_sensor_keys(
    set_core_config: SetCoreConfigCallable, key: str
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid, match=key):
        CONFIG_SCHEMA({key: {"x": {"name": "Invalid sensor"}}})
