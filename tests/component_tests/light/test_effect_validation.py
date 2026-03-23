"""Tests for light effect name validation."""

from __future__ import annotations

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


def _make_effects(*names: str) -> list[dict]:
    """Create a list of effect config dicts from names."""
    return [{f"effect_{i}": {CONF_NAME: name}} for i, name in enumerate(names)]


class TestFindEffectIndex:
    """Tests for find_effect_index helper."""

    def test_found(self) -> None:
        effects = _make_effects("Fast Pulse", "Slow Pulse")
        assert find_effect_index(effects, "Fast Pulse") == 1
        assert find_effect_index(effects, "Slow Pulse") == 2

    def test_case_insensitive(self) -> None:
        effects = _make_effects("Fast Pulse")
        assert find_effect_index(effects, "fast pulse") == 1
        assert find_effect_index(effects, "FAST PULSE") == 1

    def test_not_found(self) -> None:
        effects = _make_effects("Fast Pulse", "Slow Pulse")
        assert find_effect_index(effects, "Missing") is None

    def test_empty_effects(self) -> None:
        assert find_effect_index([], "anything") is None


class TestAvailableEffectsStr:
    """Tests for available_effects_str helper."""

    def test_multiple(self) -> None:
        effects = _make_effects("Fast Pulse", "Slow Pulse")
        assert available_effects_str(effects) == "'Fast Pulse', 'Slow Pulse'"

    def test_single(self) -> None:
        effects = _make_effects("Fast Pulse")
        assert available_effects_str(effects) == "'Fast Pulse'"

    def test_empty(self) -> None:
        assert available_effects_str([]) == "none"


class TestFinalValidateEffectRefs:
    """Tests for _final_validate effect reference checking."""

    def test_valid_effect_passes(self) -> None:
        """Valid effect name should not raise."""
        data = _get_data()
        light_id = ID("led1", is_declaration=True)
        data.effect_refs = [
            EffectRef(
                light_id=light_id,
                effect_name="Fast Pulse",
                component_path=["esphome"],
            )
        ]

        full_conf = Config()
        full_conf["light"] = [
            {
                CONF_ID: light_id,
                CONF_EFFECTS: _make_effects("Fast Pulse", "Slow Pulse"),
            }
        ]
        full_conf.declare_ids.append((light_id, ["light", 0, CONF_ID]))

        token = fv.full_config.set(full_conf)
        try:
            _final_validate({})
        finally:
            fv.full_config.reset(token)

    def test_invalid_effect_raises(self) -> None:
        """Invalid effect name should raise FinalExternalInvalid."""
        data = _get_data()
        light_id = ID("led1", is_declaration=True)
        data.effect_refs = [
            EffectRef(
                light_id=light_id,
                effect_name="Nonexistent",
                component_path=["esphome"],
            )
        ]

        full_conf = Config()
        full_conf["light"] = [
            {
                CONF_ID: light_id,
                CONF_EFFECTS: _make_effects("Fast Pulse", "Slow Pulse"),
            }
        ]
        full_conf.declare_ids.append((light_id, ["light", 0, CONF_ID]))

        token = fv.full_config.set(full_conf)
        try:
            with pytest.raises(cv.FinalExternalInvalid, match="Nonexistent"):
                _final_validate({})
        finally:
            fv.full_config.reset(token)

    def test_invalid_effect_lists_available(self) -> None:
        """Error message should list available effects."""
        data = _get_data()
        light_id = ID("led1", is_declaration=True)
        data.effect_refs = [
            EffectRef(
                light_id=light_id,
                effect_name="Missing",
                component_path=["esphome"],
            )
        ]

        full_conf = Config()
        full_conf["light"] = [
            {
                CONF_ID: light_id,
                CONF_EFFECTS: _make_effects("Fast Pulse", "Slow Pulse"),
            }
        ]
        full_conf.declare_ids.append((light_id, ["light", 0, CONF_ID]))

        token = fv.full_config.set(full_conf)
        try:
            with pytest.raises(
                cv.FinalExternalInvalid, match="'Fast Pulse', 'Slow Pulse'"
            ):
                _final_validate({})
        finally:
            fv.full_config.reset(token)

    def test_no_effects_on_light(self) -> None:
        """Light with no effects should report 'none' as available."""
        data = _get_data()
        light_id = ID("led1", is_declaration=True)
        data.effect_refs = [
            EffectRef(
                light_id=light_id,
                effect_name="Missing",
                component_path=["esphome"],
            )
        ]

        full_conf = Config()
        full_conf["light"] = [{CONF_ID: light_id}]
        full_conf.declare_ids.append((light_id, ["light", 0, CONF_ID]))

        token = fv.full_config.set(full_conf)
        try:
            with pytest.raises(
                cv.FinalExternalInvalid, match="Available effects: none"
            ):
                _final_validate({})
        finally:
            fv.full_config.reset(token)

    def test_no_refs_is_noop(self) -> None:
        """No stored refs should pass without error."""
        data = _get_data()
        data.effect_refs = []
        _final_validate({})

    def test_unknown_light_id_skipped(self) -> None:
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
            _final_validate({})  # Should not raise
        finally:
            fv.full_config.reset(token)

    def test_drains_refs(self) -> None:
        """Refs should be drained after validation to avoid redundant runs."""
        data = _get_data()
        light_id = ID("led1", is_declaration=True)
        data.effect_refs = [
            EffectRef(
                light_id=light_id,
                effect_name="Fast Pulse",
                component_path=["esphome"],
            )
        ]

        full_conf = Config()
        full_conf["light"] = [
            {
                CONF_ID: light_id,
                CONF_EFFECTS: _make_effects("Fast Pulse"),
            }
        ]
        full_conf.declare_ids.append((light_id, ["light", 0, CONF_ID]))

        token = fv.full_config.set(full_conf)
        try:
            _final_validate({})
            assert data.effect_refs == []
        finally:
            fv.full_config.reset(token)


class TestRecordEffectRef:
    """Tests for _record_effect_ref schema validator."""

    def test_records_static_effect(self) -> None:
        """Static effect name should be recorded."""
        light_id = ID("led1", is_declaration=True)
        token = path_context.set(["esphome"])
        try:
            config = {CONF_ID: light_id, CONF_EFFECT: "Fast Pulse"}
            result = _record_effect_ref(config)
            assert result is config
            data = _get_data()
            assert len(data.effect_refs) == 1
            assert data.effect_refs[0].effect_name == "Fast Pulse"
            assert data.effect_refs[0].light_id is light_id
            assert data.effect_refs[0].component_path == ["esphome"]
        finally:
            path_context.reset(token)

    def test_skips_lambda(self) -> None:
        """Lambda effect should not be recorded."""
        token = path_context.set(["esphome"])
        try:
            config = {
                CONF_ID: ID("led1", is_declaration=True),
                CONF_EFFECT: Lambda("return effect;"),
            }
            _record_effect_ref(config)
            assert _get_data().effect_refs == []
        finally:
            path_context.reset(token)

    def test_skips_none(self) -> None:
        """Effect 'None' should not be recorded."""
        token = path_context.set(["esphome"])
        try:
            config = {
                CONF_ID: ID("led1", is_declaration=True),
                CONF_EFFECT: "None",
            }
            _record_effect_ref(config)
            assert _get_data().effect_refs == []
        finally:
            path_context.reset(token)

    def test_skips_none_case_insensitive(self) -> None:
        """Effect 'none' (lowercase) should not be recorded."""
        token = path_context.set(["esphome"])
        try:
            config = {
                CONF_ID: ID("led1", is_declaration=True),
                CONF_EFFECT: "none",
            }
            _record_effect_ref(config)
            assert _get_data().effect_refs == []
        finally:
            path_context.reset(token)

    def test_skips_no_effect(self) -> None:
        """Config without effect key should be a no-op."""
        config = {CONF_ID: ID("led1", is_declaration=True)}
        _record_effect_ref(config)
        assert _get_data().effect_refs == []
