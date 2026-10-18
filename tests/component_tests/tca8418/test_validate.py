"""Tests for the tca8418 keypad and binary sensor configuration schemas."""

from __future__ import annotations

import logging

import pytest

from esphome.components.const import CONF_COLUMNS, CONF_KEYS, CONF_ROWS
from esphome.components.tca8418 import (
    CONF_GPI_EVENTS,
    CONFIG_SCHEMA as HUB_SCHEMA,
    _validate,
)
from esphome.components.tca8418.binary_sensor import (
    CONF_KEY_CODE,
    CONFIG_SCHEMA as SENSOR_SCHEMA,
)
import esphome.config_validation as cv
from esphome.const import CONF_COL, CONF_ID, CONF_KEY, CONF_NAME, CONF_ROW
from esphome.core import ID
from esphome.types import ConfigType


def _config(
    *,
    rows: int = 4,
    columns: int = 3,
    keys: str | None = None,
    gpi_events: bool = True,
) -> ConfigType:
    config: ConfigType = {CONF_GPI_EVENTS: gpi_events}
    if rows:
        config[CONF_ROWS] = rows
    if columns:
        config[CONF_COLUMNS] = columns
    if keys is not None:
        config[CONF_KEYS] = keys
    return config


def test_a_matrix_with_a_key_map_passes() -> None:
    _validate(_config(keys="123456789*0#"))


def test_individual_inputs_only_passes() -> None:
    """No matrix at all is fine as long as the pins report on their own."""
    _validate(_config(rows=0, columns=0))


@pytest.mark.parametrize("half", [{CONF_ROWS: 4}, {CONF_COLUMNS: 3}])
def test_half_a_matrix_is_rejected(half: ConfigType) -> None:
    config = {CONF_ID: ID("k", is_declaration=True), "i2c_id": ID("bus"), **half}
    with pytest.raises(cv.Invalid, match="must both be set"):
        HUB_SCHEMA(config)


def test_key_map_without_a_matrix_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="'rows' and 'columns' are required"):
        _validate(_config(rows=0, columns=0, keys="123"))


@pytest.mark.parametrize("keys", ["12345678901", "12345678"])
def test_key_map_of_the_wrong_length_is_rejected(keys: str) -> None:
    """A key map names every key of the matrix, so its length is fixed."""
    with pytest.raises(cv.Invalid, match="must have exactly 12 characters"):
        _validate(_config(keys=keys))


def test_a_keypad_that_reports_nothing_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="Nothing to report"):
        _validate(_config(rows=0, columns=0, gpi_events=False))


def test_letters_that_clash_with_individual_inputs_warn(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """97 to 114 are individual inputs and also the codes for 'a' to 'r'."""
    with caplog.at_level(logging.WARNING):
        _validate(_config(keys="abc456789*0#"))

    assert "cannot be told apart" in caplog.text
    assert "'a', 'b', 'c'" in caplog.text


def test_other_characters_do_not_warn(caplog: pytest.LogCaptureFixture) -> None:
    with caplog.at_level(logging.WARNING):
        _validate(_config(keys="123456789*0#"))

    assert caplog.text == ""


def test_no_warning_without_individual_inputs(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """With the individual inputs off there is nothing for a letter to clash with."""
    with caplog.at_level(logging.WARNING):
        _validate(_config(keys="abc456789*0#", gpi_events=False))

    assert caplog.text == ""


def _sensor(**selector: object) -> ConfigType:
    return {
        "keypad_id": ID("keypad", is_declaration=False, type="tca8418"),
        CONF_NAME: "Key",
        **selector,
    }


#  Checks made before the keypad is consulted: a key is named exactly one way,
#  a position has both halves, a character is one ASCII character, and a key
#  number is one the device uses (81 to 96 are unused, numbering stops at 114).
@pytest.mark.parametrize(
    ("selector", "error"),
    [
        ({CONF_ROW: 0, CONF_COL: 0}, None),
        ({CONF_KEY: "5"}, None),
        ({CONF_KEY: 5}, None),
        ({CONF_KEY_CODE: 12}, None),
        ({CONF_KEY_CODE: 102}, None),
        ({CONF_ROW: 0, CONF_COL: 0, CONF_KEY: "5"}, "more than one of"),
        ({CONF_ROW: 0, CONF_COL: 0, CONF_KEY_CODE: 12}, "more than one of"),
        ({CONF_KEY: "5", CONF_KEY_CODE: 12}, "more than one of"),
        ({}, "exactly one of"),
        ({CONF_ROW: 0}, "must be given together"),
        ({CONF_COL: 0}, "must be given together"),
        ({CONF_KEY: "ab"}, "single character"),
        ({CONF_KEY: ""}, "single character"),
        ({CONF_KEY: "\u00e9"}, "Only ASCII characters"),
        ({CONF_KEY_CODE: 0}, "Key numbers run from"),
        ({CONF_KEY_CODE: 81}, "Key numbers run from"),
        ({CONF_KEY_CODE: 96}, "Key numbers run from"),
        ({CONF_KEY_CODE: 115}, "Key numbers run from"),
    ],
)
def test_sensor_schema(selector: ConfigType, error: str | None) -> None:
    if error is None:
        SENSOR_SCHEMA(_sensor(**selector))
        return
    with pytest.raises(cv.Invalid, match=error):
        SENSOR_SCHEMA(_sensor(**selector))
