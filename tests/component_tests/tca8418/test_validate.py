"""Tests for the tca8418 keypad's own configuration checks.

The matrix and the individual inputs share the same pins, so the combinations
that cannot work have to be caught here rather than at run time, where they look
like a keypad that reports nothing.
"""

from __future__ import annotations

import logging

import pytest

from esphome.components.const import CONF_COLUMNS, CONF_KEYS, CONF_ROWS
from esphome.components.tca8418 import CONF_GPI_EVENTS, _validate
import esphome.config_validation as cv
from esphome.types import ConfigType


def _config(
    *,
    rows: int = 4,
    columns: int = 3,
    keys: str | None = None,
    gpi_events: bool = True,
) -> ConfigType:
    config: ConfigType = {
        CONF_ROWS: rows,
        CONF_COLUMNS: columns,
        CONF_GPI_EVENTS: gpi_events,
    }
    if keys is not None:
        config[CONF_KEYS] = keys
    return config


def test_a_matrix_with_a_key_map_passes() -> None:
    _validate(_config(keys="123456789*0#"))


def test_individual_inputs_only_passes() -> None:
    """No matrix at all is fine as long as the pins report on their own."""
    _validate(_config(rows=0, columns=0))


@pytest.mark.parametrize(("rows", "columns"), [(4, 0), (0, 3)])
def test_half_a_matrix_is_rejected(rows: int, columns: int) -> None:
    with pytest.raises(cv.Invalid, match="must both be set"):
        _validate(_config(rows=rows, columns=columns))


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
