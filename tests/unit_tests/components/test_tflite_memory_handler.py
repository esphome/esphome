"""Tests for the tflite_memory_handler configuration checks."""

import pytest

from esphome.components import tflite_memory_handler
from esphome.components.tflite_memory_handler import CONF_ARENA_LOCATION
import esphome.config_validation as cv
from esphome.const import CONF_DISABLED
import esphome.final_validate as fv


def _validate(arena_location: str, psram_config: dict | None) -> None:
    full_config = {"tflite_memory_handler": {CONF_ARENA_LOCATION: arena_location}}
    if psram_config is not None:
        full_config["psram"] = psram_config
    token = fv.full_config.set(full_config)
    try:
        tflite_memory_handler.FINAL_VALIDATE_SCHEMA(
            {CONF_ARENA_LOCATION: arena_location}
        )
    finally:
        fv.full_config.reset(token)


@pytest.mark.parametrize("arena_location", ["auto", "internal"])
def test_other_arena_locations_need_no_psram(arena_location: str) -> None:
    _validate(arena_location, None)


def test_psram_arena_without_the_component_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="requires a psram: component"):
        _validate("psram", None)


def test_psram_arena_with_a_disabled_component_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="component is disabled"):
        _validate("psram", {CONF_DISABLED: True})


def test_psram_arena_with_the_component_is_accepted() -> None:
    _validate("psram", {CONF_DISABLED: False})


def test_psram_arena_warns_when_psram_may_be_missing(caplog) -> None:
    """ignore_not_found keeps a board without PSRAM failing only at boot."""
    with caplog.at_level("WARNING", logger=tflite_memory_handler.__name__):
        _validate("psram", {CONF_DISABLED: False, "ignore_not_found": True})

    assert any("ignore_not_found" in record.getMessage() for record in caplog.records)
