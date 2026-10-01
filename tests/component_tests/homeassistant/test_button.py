"""Tests for the homeassistant button platform."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config import read_config
from esphome.core import CORE

DIR = Path(__file__).parent


def test_button_valid_domains(generate_main: Callable[[str | Path], str]) -> None:
    """Both button and input_button entities are accepted and wired up."""
    main_cpp = generate_main(DIR / "button_valid.yaml")

    assert "homeassistant::HomeassistantButton" in main_cpp
    assert 'ha_button->set_entity_id("button.example");' in main_cpp
    assert 'ha_input_button->set_entity_id("input_button.example");' in main_cpp
    assert any(d.name == "USE_API_HOMEASSISTANT_SERVICES" for d in CORE.defines)
    assert any(d.name == "USE_API_HOMEASSISTANT_STATES" for d in CORE.defines)


def test_button_invalid_domain(capsys: pytest.CaptureFixture[str]) -> None:
    """An entity from an unsupported domain is rejected."""
    CORE.config_path = DIR / "button_invalid.yaml"
    result = read_config({})

    assert result is None
    assert (
        "Entity ID sensor.example is not supported by the button platform."
        in capsys.readouterr().out
    )
