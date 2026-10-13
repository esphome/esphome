"""Tests for the selec_meter configuration schema."""

from __future__ import annotations

from typing import Any

import pytest

from esphome.components.selec_meter import sensor as selec_meter
import esphome.config_validation as cv
from esphome.types import ConfigType


def _validate(config: dict[str, Any]) -> ConfigType:
    return selec_meter.CONFIG_SCHEMA(config)


def test_model_defaults_to_em2m() -> None:
    config = _validate({"voltage": {"name": "Voltage"}})
    assert config["model"] == selec_meter.MODEL_EM2M


def test_model_is_case_insensitive() -> None:
    config = _validate({"model": "EM4M"})
    assert config["model"] == selec_meter.MODEL_EM4M


@pytest.mark.parametrize("key", ["voltage_l12", "serial_number", "dg_sensing"])
def test_em4m_only_key_rejected_without_em4m(key: str) -> None:
    with pytest.raises(cv.Invalid, match="requires 'model: em4m'"):
        _validate({key: {"name": "Test"}})


@pytest.mark.parametrize("phase", ["phase_a", "phase_b", "phase_c"])
def test_phase_key_rejected_on_default_model(phase: str) -> None:
    with pytest.raises(cv.Invalid, match="requires 'model: em4m'"):
        _validate({phase: {"voltage": {"name": "Voltage"}}})


def test_em4m_with_phases_accepted() -> None:
    config = _validate(
        {
            "model": "em4m",
            "phase_a": {"voltage": {"name": "Voltage L1"}},
            "phase_c": {"power_factor": {"name": "PF L3"}},
        }
    )
    assert "voltage" in config["phase_a"]
    assert "power_factor" in config["phase_c"]


def test_em2m_only_sensor_rejected_on_em4m() -> None:
    with pytest.raises(cv.Invalid, match="no non-DG equivalent"):
        _validate({"model": "em4m", "total_active_energy": {"name": "Total"}})


@pytest.mark.parametrize(
    ("model", "byte_order", "word_swap"),
    [
        ("em2m", "little_endian", True),
        ("em4m", "big_endian", False),
    ],
)
def test_byte_order_default_per_model(
    model: str, byte_order: str, word_swap: bool
) -> None:
    config = _validate({"model": model})
    assert config["byte_order"] == byte_order
    assert config["byte_order"].enum_value is word_swap


@pytest.mark.parametrize(
    ("model", "byte_order", "word_swap"),
    [
        ("em2m", "big_endian", False),
        ("em4m", "LITTLE_ENDIAN", True),
        ("em4m", "lsrf", True),
        ("em2m", "msrf", False),
    ],
)
def test_byte_order_explicit_override(
    model: str, byte_order: str, word_swap: bool
) -> None:
    config = _validate({"model": model, "byte_order": byte_order})
    assert config["byte_order"].enum_value is word_swap


def test_byte_order_rejects_unknown_value() -> None:
    with pytest.raises(cv.Invalid):
        _validate({"byte_order": "middle_endian"})
