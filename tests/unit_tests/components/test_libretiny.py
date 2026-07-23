"""Tests for LibreTiny board detection, including renamed-board migration."""

import pytest

from esphome.components.libretiny import _detect_variant
from esphome.components.libretiny.const import (
    FAMILY_LN882H,
    KEY_COMPONENT_DATA,
    KEY_LIBRETINY,
)
from esphome.components.ln882x import COMPONENT_DATA
import esphome.config_validation as cv
from esphome.const import CONF_BOARD, CONF_FAMILY
from esphome.core import CORE


@pytest.fixture
def ln882x_core_data() -> None:
    """Populate CORE the way the ln882x component schema does."""
    CORE.data[KEY_LIBRETINY] = {KEY_COMPONENT_DATA: COMPONENT_DATA}


def test_detect_variant_known_board_passes(ln882x_core_data: None) -> None:
    """A current board id resolves its family without warnings."""
    result = _detect_variant({CONF_BOARD: "generic-ln882h"})
    assert result[CONF_BOARD] == "generic-ln882h"
    assert result[CONF_FAMILY] == FAMILY_LN882H


def test_detect_variant_renamed_board_migrates(
    ln882x_core_data: None, caplog: pytest.LogCaptureFixture
) -> None:
    """A pre-rename board id validates against the new id, with a warning."""
    result = _detect_variant({CONF_BOARD: "generic-ln882hki"})
    assert result[CONF_BOARD] == "generic-ln882h"
    assert result[CONF_FAMILY] == FAMILY_LN882H
    assert "renamed to 'generic-ln882h'" in caplog.text


def test_detect_variant_renamed_board_does_not_mutate_input(
    ln882x_core_data: None,
) -> None:
    """Migration copies the config; the caller's dict keeps the old id."""
    value = {CONF_BOARD: "generic-ln882hki"}
    _detect_variant(value)
    assert value[CONF_BOARD] == "generic-ln882hki"


def test_detect_variant_unknown_board_still_raises(ln882x_core_data: None) -> None:
    """Ids outside the rename map keep the family-override error."""
    with pytest.raises(cv.Invalid, match="This board is unknown"):
        _detect_variant({CONF_BOARD: "not-a-real-board"})
