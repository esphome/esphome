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
    for id_, size, count in (("ha_select", 256, 16), ("ha_input_select", 64, 4)):
        storage = f"homeassistant__{id_}_buffer"
        assert f"static char {storage}__pstorage[{size}];" in main_cpp
        assert f"{storage}_2__pstorage[{count}];" in main_cpp
        assert (
            f"HomeassistantSelect({storage}__pstorage, {storage}_2__pstorage);"
            in main_cpp
        )
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
