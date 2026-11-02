"""Validate the keys, entity classes and hub reference of the LD6004 entity platforms."""

from importlib import import_module

import pytest

from esphome.components.button import ApplyButton
from esphome.components.ld6004 import LD6004Component
from esphome.components.number import ApplyNumber
from esphome.components.select import ApplySelect
from esphome.components.switch import ApplySwitch
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from esphome.core import ID
from esphome.cpp_generator import MockObjClass
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _schema(platform: str) -> cv.Schema:
    return import_module(f"esphome.components.ld6004.{platform}").CONFIG_SCHEMA


def _cases(
    platform: str,
    entity_class: MockObjClass | None,
    category: str,
    *key_sets: tuple[str, ...],
) -> list[pytest.param]:
    return [
        pytest.param(
            platform,
            keys,
            entity_class,
            category,
            id=f"{platform}-{'-'.join(keys) or 'none'}",
        )
        for keys in key_sets
    ]


@pytest.mark.parametrize(
    ("platform", "keys", "entity_class", "category"),
    [
        *_cases(
            "text_sensor",
            None,
            "diagnostic",
            (),
            ("work_mode",),
            ("ota_version",),
            ("work_mode", "ota_version"),
        ),
        *_cases(
            "select",
            ApplySelect,
            "config",
            (),
            ("work_mode",),
            ("p20_mode",),
            (
                "sensitivity",
                "trigger_speed",
                "installation_mode",
                "area_id",
                "work_mode",
                "p20_mode",
            ),
        ),
        *_cases("button", ApplyButton, "config", (), ("clear_dwell",)),
        *_cases(
            "number",
            ApplyNumber,
            "config",
            (),
            ("dwell_lifetime",),
            ("output_interval",),
            ("dwell_lifetime", "output_interval"),
        ),
        *_cases(
            "switch",
            ApplySwitch,
            "config",
            (),
            ("point_cloud",),
            ("target_display",),
            ("point_cloud", "target_display"),
        ),
    ],
)
def test_platform_keys(
    set_core_config: SetCoreConfigCallable,
    platform: str,
    keys: tuple[str, ...],
    entity_class: MockObjClass | None,
    category: str,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    config: ConfigType = _schema(platform)(
        {"ld6004_id": "radar", **{key: {"name": key} for key in keys}}
    )
    hub_id: ID = config["ld6004_id"]
    assert hub_id.type == LD6004Component
    assert str(hub_id) == "radar"
    assert set(config) == {"ld6004_id", *keys}
    for key in keys:
        if entity_class is not None:
            assert config[key]["id"].type == entity_class
        assert config[key]["entity_category"] == category
        assert "unit_of_measurement" not in config[key]


@pytest.mark.parametrize(
    ("platform", "config", "match"),
    [
        (
            "text_sensor",
            {"ld6002b_id": "radar", "work_mode": {"name": "Work Mode"}},
            "ld6002b_id",
        ),
        ("button", {"ld6004_id": "radar", "wake": {"name": "Wake"}}, "wake"),
        (
            "switch",
            {"ld6004_id": "radar", "low_power": {"name": "Low Power"}},
            "low_power",
        ),
    ],
)
def test_platform_rejects_key(
    set_core_config: SetCoreConfigCallable,
    platform: str,
    config: ConfigType,
    match: str,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid, match=match):
        _schema(platform)(config)


def test_area_config_keys(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    keys = ("x_min", "x_max", "y_min", "y_max", "z_min", "z_max")
    config: ConfigType = _schema("number")(
        {"area_config": {key: {"name": key} for key in keys}}
    )
    assert config["ld6004_id"].type == LD6004Component
    assert set(config["area_config"]) == set(keys)
    for key in keys:
        assert config["area_config"][key]["id"].type == ApplyNumber
        assert config["area_config"][key]["unit_of_measurement"] == "m"
        assert config["area_config"][key]["entity_category"] == "config"
