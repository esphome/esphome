"""Validate LD6004 button keys and hub references."""

import pytest

from esphome.components.ld600x.entities import LD600XButton
from esphome.components.ld6004 import LD6004Component
from esphome.components.ld6004.button import CONFIG_SCHEMA
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


@pytest.mark.parametrize("keys", [(), ("clear_dwell",)])
def test_model_buttons(
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
        assert config[key]["id"].type == LD600XButton
        assert config[key]["entity_category"] == "config"


def test_wake_button_rejected(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid, match="wake"):
        CONFIG_SCHEMA({"ld6004_id": "radar", "wake": {"name": "Wake"}})
