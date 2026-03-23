"""Tests for light effect name validation."""

from __future__ import annotations

from collections.abc import Generator
from contextvars import Token

import pytest

from esphome import config_validation as cv
from esphome.components.light import (
    EffectRef,
    _final_validate,
    _get_data,
    available_effects_str,
    find_effect_index,
)
from esphome.components.light.automation import _record_effect_ref
from esphome.config import Config, path_context
from esphome.const import CONF_EFFECT, CONF_EFFECTS, CONF_ID, CONF_NAME
from esphome.core import ID, Lambda
import esphome.final_validate as fv
from esphome.types import ConfigType


def _make_effects(*names: str) -> list[dict[str, dict[str, str]]]:
    """Create a list of effect config dicts from names."""
    return [{f"effect_{i}": {CONF_NAME: name}} for i, name in enumerate(names)]


# --- find_effect_index ---


def test_find_effect_index_found() -> None:
    effects = _make_effects("Fast Pulse", "Slow Pulse")
    assert find_effect_index(effects, "Fast Pulse") == 1
    assert find_effect_index(effects, "Slow Pulse") == 2


def test_find_effect_index_case_insensitive() -> None:
    effects = _make_effects("Fast Pulse")
    assert find_effect_index(effects, "fast pulse") == 1
    assert find_effect_index(effects, "FAST PULSE") == 1


def test_find_effect_index_not_found() -> None:
    effects = _make_effects("Fast Pulse", "Slow Pulse")
    assert find_effect_index(effects, "Missing") is None


def test_find_effect_index_empty() -> None:
    assert find_effect_index([], "anything") is None


# --- available_effects_str ---


def test_available_effects_str_multiple() -> None:
    effects = _make_effects("Fast Pulse", "Slow Pulse")
    assert available_effects_str(effects) == "'Fast Pulse', 'Slow Pulse'"


def test_available_effects_str_single() -> None:
    effects = _make_effects("Fast Pulse")
    assert available_effects_str(effects) == "'Fast Pulse'"


def test_available_effects_str_empty() -> None:
    assert available_effects_str([]) == "none"


# --- _final_validate ---


def _setup_final_validate(
    effect_refs: list[EffectRef],
    light_configs: list[ConfigType],
    declare_ids: list[tuple[ID, list[str | int]]],
) -> Token:
    """Set up CORE.data and fv.full_config for _final_validate tests."""
    data = _get_data()
    data.effect_refs = effect_refs

    full_conf = Config()
    full_conf["light"] = light_configs
    for id_, path in declare_ids:
        full_conf.declare_ids.append((id_, path))

    return fv.full_config.set(full_conf)


def test_final_validate_valid_effect() -> None:
    """Valid effect name should not raise."""
    light_id = ID("led1", is_declaration=True)
    token = _setup_final_validate(
        effect_refs=[
            EffectRef(
                light_id=light_id, effect_name="Fast Pulse", component_path=["esphome"]
            ),
        ],
        light_configs=[
            {CONF_ID: light_id, CONF_EFFECTS: _make_effects("Fast Pulse", "Slow Pulse")}
        ],
        declare_ids=[(light_id, ["light", 0, CONF_ID])],
    )
    try:
        _final_validate({})
    finally:
        fv.full_config.reset(token)


def test_final_validate_invalid_effect_raises() -> None:
    """Invalid effect name should raise FinalExternalInvalid."""
    light_id = ID("led1", is_declaration=True)
    token = _setup_final_validate(
        effect_refs=[
            EffectRef(
                light_id=light_id, effect_name="Nonexistent", component_path=["esphome"]
            ),
        ],
        light_configs=[
            {CONF_ID: light_id, CONF_EFFECTS: _make_effects("Fast Pulse", "Slow Pulse")}
        ],
        declare_ids=[(light_id, ["light", 0, CONF_ID])],
    )
    try:
        with pytest.raises(cv.FinalExternalInvalid, match="Nonexistent"):
            _final_validate({})
    finally:
        fv.full_config.reset(token)


def test_final_validate_lists_available_effects() -> None:
    """Error message should list available effects."""
    light_id = ID("led1", is_declaration=True)
    token = _setup_final_validate(
        effect_refs=[
            EffectRef(
                light_id=light_id, effect_name="Missing", component_path=["esphome"]
            ),
        ],
        light_configs=[
            {CONF_ID: light_id, CONF_EFFECTS: _make_effects("Fast Pulse", "Slow Pulse")}
        ],
        declare_ids=[(light_id, ["light", 0, CONF_ID])],
    )
    try:
        with pytest.raises(cv.FinalExternalInvalid, match="'Fast Pulse', 'Slow Pulse'"):
            _final_validate({})
    finally:
        fv.full_config.reset(token)


def test_final_validate_no_effects_on_light() -> None:
    """Light with no effects should report 'none' as available."""
    light_id = ID("led1", is_declaration=True)
    token = _setup_final_validate(
        effect_refs=[
            EffectRef(
                light_id=light_id, effect_name="Missing", component_path=["esphome"]
            ),
        ],
        light_configs=[{CONF_ID: light_id}],
        declare_ids=[(light_id, ["light", 0, CONF_ID])],
    )
    try:
        with pytest.raises(cv.FinalExternalInvalid, match="Available effects: none"):
            _final_validate({})
    finally:
        fv.full_config.reset(token)


def test_final_validate_no_refs_is_noop() -> None:
    """No stored refs should pass without error."""
    data = _get_data()
    data.effect_refs = []
    _final_validate({})


def test_final_validate_unknown_light_id_skipped() -> None:
    """Refs to unknown light IDs should be silently skipped."""
    data = _get_data()
    data.effect_refs = [
        EffectRef(
            light_id=ID("nonexistent", is_declaration=True),
            effect_name="Missing",
            component_path=["esphome"],
        )
    ]

    full_conf = Config()
    token = fv.full_config.set(full_conf)
    try:
        _final_validate({})
    finally:
        fv.full_config.reset(token)


def test_final_validate_drains_refs() -> None:
    """Refs should be drained after validation to avoid redundant runs."""
    light_id = ID("led1", is_declaration=True)
    token = _setup_final_validate(
        effect_refs=[
            EffectRef(
                light_id=light_id, effect_name="Fast Pulse", component_path=["esphome"]
            ),
        ],
        light_configs=[{CONF_ID: light_id, CONF_EFFECTS: _make_effects("Fast Pulse")}],
        declare_ids=[(light_id, ["light", 0, CONF_ID])],
    )
    try:
        _final_validate({})
        assert _get_data().effect_refs == []
    finally:
        fv.full_config.reset(token)


# --- _record_effect_ref ---


@pytest.fixture
def _path_ctx() -> Generator[None]:
    """Set path_context for _record_effect_ref tests."""
    token = path_context.set(["esphome"])
    yield
    path_context.reset(token)


@pytest.mark.usefixtures("_path_ctx")
def test_record_effect_ref_static() -> None:
    """Static effect name should be recorded."""
    light_id = ID("led1", is_declaration=True)
    config: ConfigType = {CONF_ID: light_id, CONF_EFFECT: "Fast Pulse"}
    result = _record_effect_ref(config)
    assert result is config
    data = _get_data()
    assert len(data.effect_refs) == 1
    assert data.effect_refs[0].effect_name == "Fast Pulse"
    assert data.effect_refs[0].light_id is light_id
    assert data.effect_refs[0].component_path == ["esphome"]


@pytest.mark.usefixtures("_path_ctx")
def test_record_effect_ref_skips_lambda() -> None:
    """Lambda effect should not be recorded."""
    config: ConfigType = {
        CONF_ID: ID("led1", is_declaration=True),
        CONF_EFFECT: Lambda("return effect;"),
    }
    _record_effect_ref(config)
    assert _get_data().effect_refs == []


@pytest.mark.usefixtures("_path_ctx")
def test_record_effect_ref_skips_none() -> None:
    """Effect 'None' should not be recorded."""
    config: ConfigType = {
        CONF_ID: ID("led1", is_declaration=True),
        CONF_EFFECT: "None",
    }
    _record_effect_ref(config)
    assert _get_data().effect_refs == []


@pytest.mark.usefixtures("_path_ctx")
def test_record_effect_ref_skips_none_case_insensitive() -> None:
    """Effect 'none' (lowercase) should not be recorded."""
    config: ConfigType = {
        CONF_ID: ID("led1", is_declaration=True),
        CONF_EFFECT: "none",
    }
    _record_effect_ref(config)
    assert _get_data().effect_refs == []


def test_record_effect_ref_skips_no_effect_key() -> None:
    """Config without effect key should be a no-op."""
    config: ConfigType = {CONF_ID: ID("led1", is_declaration=True)}
    _record_effect_ref(config)
    assert _get_data().effect_refs == []
