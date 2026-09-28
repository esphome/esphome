"""Validate LD6004 select metadata and hub references."""

import pytest

from esphome.components.ld600x.entities import LD600XSelect
from esphome.components.ld6004 import LD6004Component
from esphome.components.ld6004.select import CONFIG_SCHEMA
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


@pytest.mark.parametrize(
    "keys",
    [
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
    ],
)
def test_selects_use_ld6004_hub(
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
        assert config[key]["id"].type == LD600XSelect
        assert config[key]["entity_category"] == "config"


def test_ld6002b_hub_key_rejected(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid, match="ld6002b_id"):
        CONFIG_SCHEMA({"ld6002b_id": "radar", "work_mode": {"name": "Work Mode"}})
