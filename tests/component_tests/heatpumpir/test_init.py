"""Tests for the heatpumpir climate code generation."""

import copy

import pytest

from esphome.components.heatpumpir import climate as heatpumpir
from esphome.const import (
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_PROTOCOL,
    CONF_VISUAL,
    PlatformFramework,
)
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


@pytest.mark.asyncio
async def test_to_code_defaults_visual_before_new_climate_ir(
    monkeypatch: pytest.MonkeyPatch, set_core_config: SetCoreConfigCallable
) -> None:
    """The required min/max_temperature must seed CONF_VISUAL *before*
    new_climate_ir() reads it, otherwise the entity reports the 0-100 default
    range in Home Assistant (issue #17983)."""
    seen_visual: dict = {}

    async def fake_new_climate_ir(config: ConfigType, *args):
        # Capture the visual config exactly as new_climate_ir (and the climate
        # base it calls) would see it.
        seen_visual.update(copy.deepcopy(config.get(CONF_VISUAL, {})))
        return _RecordingVar()

    monkeypatch.setattr(heatpumpir.climate_ir, "new_climate_ir", fake_new_climate_ir)
    monkeypatch.setattr(heatpumpir.cg, "add", lambda expr: expr)
    monkeypatch.setattr(heatpumpir.cg, "add_library", lambda *a, **k: None)
    set_core_config(PlatformFramework.ESP8266_ARDUINO)

    config: ConfigType = {
        CONF_PROTOCOL: heatpumpir.PROTOCOLS["aux"],
        heatpumpir.CONF_HORIZONTAL_DEFAULT: heatpumpir.HORIZONTAL_DIRECTIONS["auto"],
        heatpumpir.CONF_VERTICAL_DEFAULT: heatpumpir.VERTICAL_DIRECTIONS["auto"],
        CONF_MIN_TEMPERATURE: 18,
        CONF_MAX_TEMPERATURE: 30,
    }

    await heatpumpir.to_code(config)

    assert seen_visual[CONF_MIN_TEMPERATURE] == 18
    assert seen_visual[CONF_MAX_TEMPERATURE] == 30


@pytest.mark.asyncio
async def test_to_code_keeps_explicit_visual(
    monkeypatch: pytest.MonkeyPatch, set_core_config: SetCoreConfigCallable
) -> None:
    """An explicit visual min/max is not overwritten by the required temps."""
    seen_visual: dict = {}

    async def fake_new_climate_ir(config: ConfigType, *args):
        seen_visual.update(copy.deepcopy(config.get(CONF_VISUAL, {})))
        return _RecordingVar()

    monkeypatch.setattr(heatpumpir.climate_ir, "new_climate_ir", fake_new_climate_ir)
    monkeypatch.setattr(heatpumpir.cg, "add", lambda expr: expr)
    monkeypatch.setattr(heatpumpir.cg, "add_library", lambda *a, **k: None)
    set_core_config(PlatformFramework.ESP8266_ARDUINO)

    config: ConfigType = {
        CONF_PROTOCOL: heatpumpir.PROTOCOLS["aux"],
        heatpumpir.CONF_HORIZONTAL_DEFAULT: heatpumpir.HORIZONTAL_DIRECTIONS["auto"],
        heatpumpir.CONF_VERTICAL_DEFAULT: heatpumpir.VERTICAL_DIRECTIONS["auto"],
        CONF_MIN_TEMPERATURE: 16,
        CONF_MAX_TEMPERATURE: 32,
        CONF_VISUAL: {CONF_MIN_TEMPERATURE: 18, CONF_MAX_TEMPERATURE: 30},
    }

    await heatpumpir.to_code(config)

    assert seen_visual[CONF_MIN_TEMPERATURE] == 18
    assert seen_visual[CONF_MAX_TEMPERATURE] == 30


class _RecordingVar:
    """A stand-in for the climate var whose setter calls are no-ops."""

    def __getattr__(self, _name: str):
        return lambda *args, **kwargs: None
