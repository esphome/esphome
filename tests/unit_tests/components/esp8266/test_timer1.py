"""Two components that program hardware timer 1 are refused."""

import asyncio

import pytest

from esphome.components.esp8266.const import (
    KEY_ESP8266,
    check_timer1_users,
    require_waveform,
    use_timer1,
)
from esphome.const import KEY_CORE, KEY_TARGET_PLATFORM
from esphome.core import CORE, EsphomeError


@pytest.fixture(autouse=True)
def esp8266_core() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "esp8266"}
    CORE.data[KEY_ESP8266] = {}


def _check() -> None:
    asyncio.run(check_timer1_users())


def test_one_user_passes() -> None:
    use_timer1("opentherm")
    use_timer1("opentherm")
    _check()


def test_two_users_are_refused() -> None:
    use_timer1("opentherm")
    use_timer1("hdmi_cec")
    with pytest.raises(
        EsphomeError, match="hdmi_cec and opentherm each need hardware timer 1"
    ):
        _check()


def test_waveform_outputs_share_the_timer_but_not_with_a_direct_user() -> None:
    require_waveform()  # esp8266_pwm
    require_waveform()  # ac_dimmer
    _check()
    use_timer1("lightwaverf")
    with pytest.raises(EsphomeError, match="waveform generator"):
        _check()
