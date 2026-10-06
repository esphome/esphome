"""Tests that light codegen skips setters for default values."""

from collections.abc import Callable
from pathlib import Path
import re


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
