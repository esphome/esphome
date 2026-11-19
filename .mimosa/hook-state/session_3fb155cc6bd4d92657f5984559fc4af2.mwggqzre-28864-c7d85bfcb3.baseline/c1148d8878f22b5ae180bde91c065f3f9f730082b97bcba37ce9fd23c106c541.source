"""Tests for LibreTiny board detection, including renamed-board migration."""

import pytest

from esphome.components import bk72xx, ln882x, rtl87xx
from esphome.components.libretiny import BASE_SCHEMA, _detect_variant
from esphome.components.libretiny.const import (
    FAMILY_LN882H,
    KEY_COMPONENT_DATA,
    KEY_LIBRETINY,
)
from esphome.components.ln882x import COMPONENT_DATA
import esphome.config_validation as cv
from esphome.const import CONF_BOARD, CONF_FAMILY
from esphome.core import CORE, KEY_CORE


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


def test_platform_schemas_are_isolated_instances() -> None:
    """Each LibreTiny platform must own its CONFIG_SCHEMA instance.

    BASE_SCHEMA is shared; every platform prepends its own _set_core_data
    extra. On the shared object, importing two platform modules in one process
    made either platform's validation run both extras, so the wrong platform's
    component data won and known boards failed to resolve.
    """
    platforms = (bk72xx, ln882x, rtl87xx)
    schemas = [platform.CONFIG_SCHEMA for platform in platforms]
    assert len({id(schema) for schema in (BASE_SCHEMA, *schemas)}) == 4
    # The shared base must not have accumulated any platform's extra.
    # prepend_extra wraps validators in _Schema, so unwrap before comparing.
    base_extras = [extra.schema for extra in BASE_SCHEMA._extra_schemas]
    for platform in platforms:
        assert platform._set_core_data not in base_extras


def test_each_platform_resolves_its_own_boards() -> None:
    """Validating one platform's config must leave that platform's component
    data in CORE.data. On the shared schema, the last-imported platform's
    _set_core_data won for every platform, so known boards failed to resolve
    with "This board is unknown"."""
    CORE.data[KEY_CORE] = {}  # written by the schema's _update_core_data extra
    for platform, board in (
        (ln882x, "generic-ln882h"),
        (bk72xx, "generic-bk7252"),
        (rtl87xx, "generic-rtl8720cf-2mb-896k"),
    ):
        platform.CONFIG_SCHEMA({CONF_BOARD: board})
        assert CORE.data[KEY_LIBRETINY][KEY_COMPONENT_DATA] is platform.COMPONENT_DATA
