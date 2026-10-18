"""Tests for the lp5562 duplicate channel check."""

import pytest

from esphome import config_validation as cv
from esphome.components.lp5562.output import CONF_LP5562_ID, _final_validate
from esphome.config import Config
from esphome.const import CONF_CHANNEL, PlatformFramework
from esphome.core import ID

from ..types import SetCoreConfigCallable

RED = "red"
BLUE = "blue"


def _output(hub: str, channel: str) -> dict:
    return {CONF_LP5562_ID: ID(hub), CONF_CHANNEL: channel}


def test_each_channel_once_is_accepted(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=Config())
    _final_validate(_output("lp5562_1", RED))
    _final_validate(_output("lp5562_1", BLUE))
    # Same channel on a second chip is fine
    _final_validate(_output("lp5562_2", RED))


def test_same_channel_twice_is_rejected(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=Config())
    _final_validate(_output("lp5562_1", RED))
    with pytest.raises(cv.Invalid, match="Channel 'red' of 'lp5562_1' is already used"):
        _final_validate(_output("lp5562_1", RED))
