"""Tests for beken_irda configuration validation."""

import pytest

from esphome.components.beken_irda import validate_hardware_board, validate_hardware_pin
from esphome.components.libretiny.const import KEY_BOARD, KEY_LIBRETINY
import esphome.config_validation as cv
from esphome.const import CONF_NUMBER
from esphome.core import CORE


def test_hardware_pin_accepts_p26() -> None:
    """Hardware mode uses the fixed IRDA pin."""
    assert validate_hardware_pin({CONF_NUMBER: 26})[CONF_NUMBER] == 26


def test_hardware_pin_rejects_other_pins() -> None:
    """Hardware mode has no pin matrix, so any pin but P26 is rejected."""
    with pytest.raises(cv.Invalid, match="P26"):
        validate_hardware_pin({CONF_NUMBER: 14})


def test_hardware_board_accepts_legacy_block() -> None:
    """The legacy NEC register set is available on the BK7231 family."""
    CORE.data[KEY_LIBRETINY] = {KEY_BOARD: "generic-bk7231t-qfn32-tuya"}
    assert validate_hardware_board({}) == {}


def test_hardware_board_rejects_bk7252n() -> None:
    """BK7252N/BK7253 use a different IRDA block and must not reach the legacy registers."""
    CORE.data[KEY_LIBRETINY] = {KEY_BOARD: "generic-bk7252n"}
    with pytest.raises(cv.Invalid, match="not supported"):
        validate_hardware_board({})
