"""Tests for the homeassistant button platform."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome.config import read_config
from esphome.core import CORE

DIR = Path(__file__).parent


def test_button_valid_domains(generate_main: Callable[[str | Path], str]) -> None:
    """Both button and input_button entities are accepted and wired up."""
    main_cpp = generate_main(DIR / "button_valid.yaml")

    for button_id, entity_id in (
        ("ha_button", "button.example"),
        ("ha_input_button", "input_button.example"),
    ):
        fn = re.search(rf"new\({button_id}\) button::ApplyButton<(\w+)>\(\);", main_cpp)
        assert fn is not None
        assert (
            f"static void {fn.group(1)}(button::Button * entity) {{\n"
            f'  homeassistant::press_button_entity(entity, "{entity_id}");\n}}'
        ) in main_cpp
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
