"""Tests for beken_irda configuration validation."""

import pytest

from esphome.components.beken_irda import validate_board, validate_pin
from esphome.components.libretiny.const import (
    FAMILY_BK7231Q,
    FAMILY_BK7231T,
    FAMILY_BK7238,
    FAMILY_BK7251,
    KEY_BOARD,
    KEY_FAMILY,
    KEY_LIBRETINY,
)
import esphome.config_validation as cv
from esphome.const import CONF_INPUT, CONF_INVERTED, CONF_MODE, CONF_NUMBER, CONF_PULLUP
from esphome.core import CORE


def _set_board(family: str, board: str = "generic-board") -> None:
    CORE.data[KEY_LIBRETINY] = {KEY_FAMILY: family, KEY_BOARD: board}


def _pin(number: int = 26, inverted: bool = False, pullup: bool = False) -> dict:
    return {
        CONF_NUMBER: number,
        CONF_INVERTED: inverted,
        CONF_MODE: {CONF_INPUT: True, CONF_PULLUP: pullup},
    }


def test_pin_accepts_plain_p26() -> None:
    """The default input pin mode is what Irda_init_app() sets up."""
    assert validate_pin(_pin()) == _pin()


def test_pin_rejects_other_pins() -> None:
    """The IRDA block has no pin matrix."""
    with pytest.raises(cv.Invalid, match="P26"):
        validate_pin(_pin(number=14))


@pytest.mark.parametrize("pin", [_pin(inverted=True), _pin(pullup=True)])
def test_pin_rejects_modes_and_inversion(pin: dict) -> None:
    """The block would silently ignore them."""
    with pytest.raises(cv.Invalid, match="pin modes or inversion"):
        validate_pin(pin)


def test_board_accepts_legacy_block() -> None:
    """The legacy NEC register set is available on the BK7231 family."""
    _set_board(FAMILY_BK7231T, "generic-bk7231t-qfn32-tuya")
    assert validate_board({}) == {}


@pytest.mark.parametrize(
    ("family", "board"),
    [
        (FAMILY_BK7238, "generic-bk7238"),
        (FAMILY_BK7231Q, "generic-bk7231q"),
        (FAMILY_BK7251, "generic-bk7252n"),
        (FAMILY_BK7251, "generic-bk7253"),
    ],
)
def test_board_rejects_parts_without_legacy_block(family: str, board: str) -> None:
    """No IRDA block, no IRDA interrupt, or a different block."""
    _set_board(family, board)
    with pytest.raises(cv.Invalid, match="not supported"):
        validate_board({})
