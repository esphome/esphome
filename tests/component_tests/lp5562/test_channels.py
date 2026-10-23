"""Tests for the lp5562 duplicate channel check."""

import pytest

from esphome import config_validation as cv
from esphome.components.lp5562.output import CONF_LP5562_ID, _final_validate
from esphome.config import Config
from esphome.const import CONF_CHANNEL, CONF_PLATFORM, PlatformFramework
from esphome.core import ID

from ..types import SetCoreConfigCallable

RED = "red"
BLUE = "blue"


def _output(hub: str, channel: str) -> dict:
    return {CONF_PLATFORM: "lp5562", CONF_LP5562_ID: ID(hub), CONF_CHANNEL: channel}


def _set_outputs(set_core_config: SetCoreConfigCallable, *outputs: dict) -> None:
    full = Config()
    full["output"] = list(outputs)
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full)


def test_each_channel_once_is_accepted(set_core_config: SetCoreConfigCallable) -> None:
    # Same channel on a second chip is fine
    outputs = (
        _output("lp5562_1", RED),
        _output("lp5562_1", BLUE),
        _output("lp5562_2", RED),
    )
    _set_outputs(set_core_config, *outputs)
    for config in outputs:
        _final_validate(config)


def test_same_channel_twice_is_rejected(set_core_config: SetCoreConfigCallable) -> None:
    outputs = (_output("lp5562_1", RED), _output("lp5562_1", RED))
    _set_outputs(set_core_config, *outputs)
    with pytest.raises(cv.Invalid, match="Channel 'red' of 'lp5562_1' is already used"):
        _final_validate(outputs[1])
