"""Tests for the heatpumpir climate config validation."""

from esphome.components.heatpumpir.climate import _default_visual
from esphome.const import CONF_MAX_TEMPERATURE, CONF_MIN_TEMPERATURE, CONF_VISUAL
from esphome.types import ConfigType


def test_default_visual_seeds_from_required_min_max() -> None:
    """Without a visual block, the required min/max_temperature seed the visual
    range so the entity reports it in Home Assistant instead of 0-100 (#17983)."""
    config: ConfigType = {CONF_MIN_TEMPERATURE: 18, CONF_MAX_TEMPERATURE: 30}
    _default_visual(config)
    assert config[CONF_VISUAL][CONF_MIN_TEMPERATURE] == 18
    assert config[CONF_VISUAL][CONF_MAX_TEMPERATURE] == 30


def test_default_visual_keeps_explicit() -> None:
    """An explicit visual min/max is not overwritten by the required temps."""
    config: ConfigType = {
        CONF_MIN_TEMPERATURE: 16,
        CONF_MAX_TEMPERATURE: 32,
        CONF_VISUAL: {CONF_MIN_TEMPERATURE: 18, CONF_MAX_TEMPERATURE: 30},
    }
    _default_visual(config)
    assert config[CONF_VISUAL][CONF_MIN_TEMPERATURE] == 18
    assert config[CONF_VISUAL][CONF_MAX_TEMPERATURE] == 30
