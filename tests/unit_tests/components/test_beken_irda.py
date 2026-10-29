"""Tests for beken_irda configuration validation."""

import pytest

from esphome.components.beken_irda import MODE_HARDWARE, MODE_TIMER, validate_config
from esphome.components.libretiny.const import KEY_BOARD, KEY_LIBRETINY
import esphome.config_validation as cv
from esphome.const import CONF_MODE, CONF_NUMBER, CONF_PIN
from esphome.core import CORE


def _config(*, mode: str = MODE_TIMER, pin: int = 26) -> dict:
    return {CONF_MODE: mode, CONF_PIN: {CONF_NUMBER: pin}}


def _set_board(board: str) -> None:
    CORE.data[KEY_LIBRETINY] = {KEY_BOARD: board}


def test_hardware_mode_on_p26_passes() -> None:
    """Hardware mode accepts the fixed IRDA pin on a legacy board."""
    _set_board("generic-bk7231t-qfn32-tuya")
    assert validate_config(_config(mode=MODE_HARDWARE))[CONF_MODE] == MODE_HARDWARE


def test_hardware_mode_rejects_other_pins() -> None:
    """Hardware mode has no pin matrix, so any pin but P26 is rejected."""
    _set_board("generic-bk7231t-qfn32-tuya")
    with pytest.raises(cv.Invalid, match="P26"):
        validate_config(_config(mode=MODE_HARDWARE, pin=14))


def test_hardware_mode_rejected_on_bk7252n() -> None:
    """BK7252N/BK7253 use a different IRDA block and must not reach the legacy registers."""
    _set_board("generic-bk7252n")
    with pytest.raises(cv.Invalid, match="not supported"):
        validate_config(_config(mode=MODE_HARDWARE))


def test_timer_mode_accepts_any_pin() -> None:
    """Timer mode samples a plain input, so a non-P26 pin is valid."""
    _set_board("generic-bk7231n-qfn32-tuya")
    assert validate_config(_config(pin=14))[CONF_PIN][CONF_NUMBER] == 14
