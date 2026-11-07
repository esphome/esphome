"""Tests for the homeassistant select platform."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config import read_config
from esphome.core import CORE

DIR = Path(__file__).parent


def test_select_valid_domains(generate_main: Callable[[str | Path], str]) -> None:
    """Both select and input_select entities are accepted and wired up."""
    main_cpp = generate_main(DIR / "select_valid.yaml")

    # Default and configured option storage are passed to the constructor
    assert "new(ha_select) homeassistant::HomeassistantSelect(16, 256);" in main_cpp
    assert "new(ha_input_select) homeassistant::HomeassistantSelect(4, 64);" in main_cpp
    assert 'ha_select->set_entity_id("select.example");' in main_cpp
    assert 'ha_input_select->set_entity_id("input_select.example");' in main_cpp
    assert any(d.name == "USE_API_HOMEASSISTANT_SERVICES" for d in CORE.defines)
    assert any(d.name == "USE_API_HOMEASSISTANT_STATES" for d in CORE.defines)


def test_select_invalid_domain(capsys: pytest.CaptureFixture[str]) -> None:
    """An entity from an unsupported domain is rejected."""
    CORE.config_path = DIR / "select_invalid.yaml"
    result = read_config({})

    assert result is None
    assert (
        "Entity ID sensor.example is not supported by the select platform."
        in capsys.readouterr().out
    )
