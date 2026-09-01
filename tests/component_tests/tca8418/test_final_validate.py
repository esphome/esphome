"""Tests for the tca8418 binary sensor's final validation.

A key can be named by its position, by the character it is mapped to, or by the
number the device reports, and each one is turned into a key number here. Getting
that arithmetic wrong, or letting through a key the keypad cannot report, gives a
sensor that never fires rather than an error, so these are the cases worth
pinning down.
"""

from __future__ import annotations

import pytest

from esphome.components.const import CONF_COLUMNS, CONF_KEYS, CONF_ROWS
from esphome.components.tca8418 import CONF_GPI_EVENTS
from esphome.components.tca8418.binary_sensor import (
    CONF_KEY_CODE,
    CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA,
)
from esphome.config import Config
import esphome.config_validation as cv
from esphome.const import (
    CONF_COL,
    CONF_ID,
    CONF_KEY,
    CONF_NAME,
    CONF_ROW,
    PlatformFramework,
)
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

HUB_ID = "keypad"


def _full_config(
    *,
    rows: int = 4,
    columns: int = 3,
    keys: str | None = "123456789*0#",
    gpi_events: bool = True,
) -> Config:
    """A full config carrying one keypad, as the ID pass leaves it."""
    hub: ConfigType = {
        CONF_ID: ID(HUB_ID, is_declaration=True, type="tca8418"),
        CONF_ROWS: rows,
        CONF_COLUMNS: columns,
        CONF_GPI_EVENTS: gpi_events,
    }
    if keys is not None:
        hub[CONF_KEYS] = keys
    full = Config()
    full["tca8418"] = [hub]
    full.declare_ids.append((hub[CONF_ID], ["tca8418", 0, CONF_ID]))
    return full


def _sensor(**selector: object) -> ConfigType:
    config: ConfigType = {
        "keypad_id": ID(HUB_ID, is_declaration=False, type="tca8418"),
        CONF_NAME: "Key",
    }
    config.update(selector)
    return config


def _validated(config: ConfigType) -> ConfigType:
    config = CONFIG_SCHEMA(config)
    FINAL_VALIDATE_SCHEMA(config)
    return config


@pytest.fixture
def keypad(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=_full_config())


#  A key number is worked out here so the sensor only compares numbers at run
#  time. Matrix keys are row * 10 + column + 1.
@pytest.mark.parametrize(
    ("selector", "key_code"),
    [
        ({CONF_ROW: 0, CONF_COL: 0}, 1),
        ({CONF_ROW: 3, CONF_COL: 2}, 33),
        ({CONF_KEY: "1"}, 1),
        ({CONF_KEY: "5"}, 12),
        ({CONF_KEY: "#"}, 33),
        ({CONF_KEY_CODE: 12}, 12),
    ],
)
def test_key_is_resolved(keypad: None, selector: ConfigType, key_code: int) -> None:
    assert _validated(_sensor(**selector))[CONF_KEY_CODE] == key_code


def test_position_outside_the_matrix_is_rejected(keypad: None) -> None:
    with pytest.raises(cv.Invalid, match="outside the 4 x 3 key matrix"):
        _validated(_sensor(row=0, col=3))


def test_key_number_outside_the_matrix_is_rejected(keypad: None) -> None:
    """Key 5 is ROW0/COL4, which a three column matrix never reports."""
    with pytest.raises(cv.Invalid, match="position 0/4, which is outside"):
        _validated(_sensor(key_code=5))


def test_key_number_of_a_pin_the_matrix_uses_is_rejected(keypad: None) -> None:
    """97 is ROW0, which belongs to the matrix, so it cannot report on its own."""
    with pytest.raises(cv.Invalid, match="ROW0, which the key matrix is using"):
        _validated(_sensor(key_code=97))


def test_individual_input_outside_the_matrix_passes(keypad: None) -> None:
    """ROW5 is past the four rows of the matrix, so it reports on its own."""
    assert _validated(_sensor(key_code=102))[CONF_KEY_CODE] == 102


def test_column_beyond_the_matrix_passes(keypad: None) -> None:
    """COL3 is past the three columns of the matrix; 105 is COL0."""
    assert _validated(_sensor(key_code=108))[CONF_KEY_CODE] == 108


def test_individual_input_needs_gpi_events(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=_full_config(gpi_events=False)
    )

    with pytest.raises(cv.Invalid, match="'gpi_events' turned off"):
        _validated(_sensor(key_code=102))


def test_character_without_a_key_map_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config=_full_config(keys=None))

    with pytest.raises(cv.Invalid, match="which is not set"):
        _validated(_sensor(key="5"))


def test_character_not_in_the_key_map_is_rejected(keypad: None) -> None:
    with pytest.raises(cv.Invalid, match="not one of the keypad's keys"):
        _validated(_sensor(key="Z"))


def test_repeated_character_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A character appearing twice does not name one key."""
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=_full_config(keys="1234567891*0")
    )

    with pytest.raises(cv.Invalid, match="appears more than once"):
        _validated(_sensor(key="1"))


def test_key_given_as_a_number_is_accepted(keypad: None) -> None:
    """'key: 5' is a reasonable way to name the key labelled 5."""
    assert _validated(_sensor(key=5))[CONF_KEY_CODE] == 12
