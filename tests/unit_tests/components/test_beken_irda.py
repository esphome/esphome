"""Tests for beken_irda configuration validation."""

import logging

import pytest

from esphome import final_validate as fv
from esphome.components.beken_irda import (
    MODE_HARDWARE,
    MODE_TIMER,
    final_validate,
    validate_config,
    validate_hardware_board,
    validate_hardware_pin,
    validate_pin,
)
from esphome.components.libretiny.const import (
    FAMILY_BK7231T,
    FAMILY_BK7238,
    KEY_BOARD,
    KEY_FAMILY,
    KEY_LIBRETINY,
)
import esphome.config_validation as cv
from esphome.const import CONF_FILTER, CONF_INVERTED, CONF_MODE, CONF_NUMBER
from esphome.core import CORE, TimePeriod


def _set_family(family: str) -> None:
    CORE.data[KEY_LIBRETINY] = {KEY_FAMILY: family, KEY_BOARD: "generic-board"}


def test_pin_accepts_non_inverted() -> None:
    """The active-low receiver is read as-is."""
    assert validate_pin({CONF_INVERTED: False}) == {CONF_INVERTED: False}


def test_pin_rejects_inverted() -> None:
    """Neither frontend applies the pin's inversion setting."""
    with pytest.raises(cv.Invalid, match="inverted"):
        validate_pin({CONF_INVERTED: True})


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


def test_filter_warning_uses_default_sample_period(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A filter below the default 100us sample period is reported without sample_period."""
    with caplog.at_level(logging.WARNING):
        validate_config(
            {CONF_MODE: MODE_TIMER, CONF_FILTER: TimePeriod(microseconds=80)}
        )
    assert "filter is shorter" in caplog.text


def test_filter_warning_skipped_without_filter(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The default filter is not a user error."""
    with caplog.at_level(logging.WARNING):
        validate_config({CONF_MODE: MODE_TIMER})
    assert caplog.text == ""


def test_final_validate_rejects_timer_with_bk7238_transmitter() -> None:
    """On BK7238 the transmitter's envelope owns BKTIMER1, so timer mode cannot share it."""
    _set_family(FAMILY_BK7238)
    token = fv.full_config.set({"remote_transmitter": {}})
    try:
        with pytest.raises(cv.Invalid, match="BKTIMER1"):
            final_validate({CONF_MODE: MODE_TIMER})
    finally:
        fv.full_config.reset(token)


def test_final_validate_allows_hardware_with_bk7238_transmitter() -> None:
    """Hardware mode does not use the timer, so it can coexist with the transmitter."""
    _set_family(FAMILY_BK7238)
    token = fv.full_config.set({"remote_transmitter": {}})
    try:
        final_validate({CONF_MODE: MODE_HARDWARE})
    finally:
        fv.full_config.reset(token)


def test_final_validate_allows_timer_without_transmitter() -> None:
    """Without a transmitter there is no channel conflict."""
    _set_family(FAMILY_BK7238)
    token = fv.full_config.set({})
    try:
        final_validate({CONF_MODE: MODE_TIMER})
    finally:
        fv.full_config.reset(token)


def test_final_validate_allows_timer_on_other_families() -> None:
    """Only BK7238 uses BKTIMER1 for the transmitter envelope."""
    _set_family(FAMILY_BK7231T)
    token = fv.full_config.set({"remote_transmitter": {}})
    try:
        final_validate({CONF_MODE: MODE_TIMER})
    finally:
        fv.full_config.reset(token)
