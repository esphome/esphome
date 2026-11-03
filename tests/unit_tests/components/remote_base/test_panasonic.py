"""A Panasonic address_2 is only sent with a 56-bit code."""

from __future__ import annotations

import pytest

from esphome.automation import ACTION_REGISTRY
from esphome.components.remote_base import (
    CONF_ADDRESS_2,
    PANASONIC_SCHEMA,
    _panasonic_address_2_needs_56_bits,
)
import esphome.config_validation as cv
from esphome.const import CONF_NBITS
from esphome.core import Lambda

BASE = {"address": 0x4004, "command": 0x0100BCBD}


def test_address_2_needs_56_bits() -> None:
    config = PANASONIC_SCHEMA({**BASE, CONF_NBITS: 56, CONF_ADDRESS_2: 0x12})
    assert _panasonic_address_2_needs_56_bits(config)[CONF_ADDRESS_2] == 0x12
    with pytest.raises(cv.Invalid, match="only sent with nbits: 56"):
        _panasonic_address_2_needs_56_bits(
            PANASONIC_SCHEMA({**BASE, CONF_NBITS: 48, CONF_ADDRESS_2: 0x12})
        )


def test_zero_address_2_is_fine_with_48_bits() -> None:
    config = PANASONIC_SCHEMA({**BASE, CONF_ADDRESS_2: 0})
    assert _panasonic_address_2_needs_56_bits(config)[CONF_NBITS] == 48


def test_lambda_address_2_is_not_checked_here() -> None:
    config = {**BASE, CONF_NBITS: 48, CONF_ADDRESS_2: Lambda("return 0x12;")}
    assert isinstance(
        _panasonic_address_2_needs_56_bits(config)[CONF_ADDRESS_2], Lambda
    )


def test_transmit_action_checks_address_2() -> None:
    schema = ACTION_REGISTRY["remote_transmitter.transmit_panasonic"].raw_schema
    assert schema({**BASE, CONF_NBITS: 56, CONF_ADDRESS_2: 0x12})[CONF_NBITS] == 56
    assert isinstance(
        schema({**BASE, CONF_ADDRESS_2: Lambda("return 0x12;")})[CONF_ADDRESS_2],
        Lambda,
    )
    with pytest.raises(cv.Invalid, match="only sent with nbits: 56"):
        schema({**BASE, CONF_ADDRESS_2: 0x12})
