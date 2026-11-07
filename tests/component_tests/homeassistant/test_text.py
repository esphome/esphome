"""Tests for the homeassistant text platform."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.config import read_config
from esphome.core import CORE

DIR = Path(__file__).parent


def test_text_valid_domains(generate_main: Callable[[str | Path], str]) -> None:
    """Both text and input_text entities are accepted and wired up."""
    main_cpp = generate_main(DIR / "text_valid.yaml")

    assert "homeassistant::HomeassistantText" in main_cpp
    assert 'ha_text->set_entity_id("text.example");' in main_cpp
    assert 'ha_input_text->set_entity_id("input_text.example");' in main_cpp
    assert "ha_text->traits.set_mode(text::TEXT_MODE_TEXT);" in main_cpp
    assert "ha_text->traits.set_min_length(0);" in main_cpp
    assert "ha_text->traits.set_max_length(255);" in main_cpp
    assert any(d.name == "USE_API_HOMEASSISTANT_SERVICES" for d in CORE.defines)
    assert any(d.name == "USE_API_HOMEASSISTANT_STATES" for d in CORE.defines)


def test_text_invalid_domain(capsys: pytest.CaptureFixture[str]) -> None:
    """An entity from an unsupported domain is rejected."""
    CORE.config_path = DIR / "text_invalid.yaml"
    result = read_config({})

    assert result is None
    assert (
        "Entity ID sensor.example is not supported by the text platform."
        in capsys.readouterr().out
    )
