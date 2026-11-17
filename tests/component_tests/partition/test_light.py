"""Partition light schema."""

import pytest

from esphome import config_validation as cv
from esphome.components.light import CONF_POWER_ESTIMATION
from esphome.components.partition.light import CONFIG_SCHEMA
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def test_partition_rejects_its_own_power_estimation(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid, match="belongs on the strip") as exc:
        CONFIG_SCHEMA(
            {
                "name": "Part",
                "segments": [{"id": "strip", "from": 0, "to": 3}],
                CONF_POWER_ESTIMATION: {
                    "ma_per_led_red": 16.0,
                    "ma_per_led_green": 11.0,
                    "ma_per_led_blue": 15.0,
                    "idle_ma_per_led": 1.0,
                },
            }
        )
    assert exc.value.path == [CONF_POWER_ESTIMATION]
