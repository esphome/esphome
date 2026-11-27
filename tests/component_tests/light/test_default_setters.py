"""Tests that light codegen skips setters for default values."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

import esphome.codegen as cg
from esphome.components import light
from esphome.components.light import types as light_types
from esphome.core import CORE, ID, EsphomeError


def test_default_flash_length_and_empty_effects_are_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A 0 ms flash transition and an empty effect list match the C++ defaults."""
    main_cpp = generate_main(component_config_path("transitions.yaml"))

    assert "plain_light->set_flash_transition_length(" not in main_cpp
    assert "plain_light->add_effects(" not in main_cpp
    assert "bare_light->set_flash_transition_length(" not in main_cpp
    assert "bare_light->add_effects(" not in main_cpp
    assert "fancy_light->set_flash_transition_length(500);" in main_cpp
    call = re.search(r"fancy_light->add_effects\((\w+), (\d+)\);", main_cpp)
    assert call is not None
    # The effect pointers form a flash table; they are address constants, not constexpr
    assert re.search(
        rf"static light::LightEffect \* const {call.group(1)}\[\] PROGMEM = \{{[^}}]+\}};",
        main_cpp,
    )
    assert "USE_LIGHT_EFFECTS" in {define.name for define in CORE.defines}


def test_effects_compile_out_without_any_effect(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """No light with effects leaves USE_LIGHT_EFFECTS undefined, so LightState drops its effect list."""
    generate_main(component_config_path("write_on_setup.yaml"))

    assert "USE_LIGHT_EFFECTS" not in {define.name for define in CORE.defines}


def test_effect_assigned_in_setup_is_rejected() -> None:
    """A pointer assigned in setup() would make the flash table need dynamic init."""
    dynamic_effect = cg.Pvariable(
        ID("dynamic_effect", is_declaration=True, type=light_types.LightEffect),
        cg.RawExpression("make_effect()"),
    )
    with pytest.raises(EsphomeError, match="dynamic_effect"):
        light._effects_table([dynamic_effect])
