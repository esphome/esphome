"""Validate inference task affinity before generating firmware."""

import pytest

from esphome.components import micro_wake_word as mww
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_ESP32, KEY_TARGET_PLATFORM, KEY_VARIANT
from esphome.core import CORE
import esphome.final_validate as fv


@pytest.mark.parametrize(
    "variant", ["ESP32", "ESP32S3", "ESP32P4", "ESP32S2", "ESP32C3"]
)
@pytest.mark.parametrize("core", [-1, 0, 1])
def test_task_core_matches_target(variant: str, core: int) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    CORE.data[KEY_ESP32] = {KEY_VARIANT: variant}
    token = fv.full_config.set({"esp32": {"framework": {}}})
    try:
        config = mww.CONFIG_SCHEMA({"task_core": core})
        if core == 1 and variant in ("ESP32S2", "ESP32C3"):
            with pytest.raises(cv.Invalid, match="only available"):
                mww._validate_task_core(config)
        else:
            assert mww._validate_task_core(config)["task_core"] == core
    finally:
        fv.full_config.reset(token)


@pytest.mark.parametrize("core", [-2, 2])
def test_invalid_task_core(core: int) -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    with pytest.raises(cv.Invalid):
        mww.CONFIG_SCHEMA({"task_core": core})


def test_default_task_core_is_unpinned() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    assert mww.CONFIG_SCHEMA({})["task_core"] == -1


def test_core_one_rejected_when_freertos_is_unicore() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp32"}
    CORE.data[KEY_ESP32] = {KEY_VARIANT: "ESP32S3"}
    token = fv.full_config.set(
        {
            "esp32": {
                "framework": {"sdkconfig_options": {"CONFIG_FREERTOS_UNICORE": "y"}}
            }
        }
    )
    try:
        with pytest.raises(cv.Invalid, match="dual-core"):
            mww._validate_task_core({"task_core": 1})
    finally:
        fv.full_config.reset(token)
