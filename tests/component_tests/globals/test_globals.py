"""Tests for the globals component."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path


def test_globals_placement_new_with_template_args(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test that globals uses placement new with template arguments preserved."""
    main_cpp = generate_main(component_config_path("globals_test.yaml"))

    # Globals uses Pvariable with Type.new(template_args, initial_value)
    # which exercises the template_args preservation in placement new.
    assert "static globals::GlobalsComponent<int> *const my_global_int" in main_cpp
    assert "sizeof(globals::GlobalsComponent<int>)" in main_cpp
    assert "new(my_global_int) globals::GlobalsComponent<int>" in main_cpp

    # Verify initial value is passed as constructor arg
    assert "42" in main_cpp

    # Check other globals are also generated
    assert "sizeof(globals::GlobalsComponent<float>)" in main_cpp
    assert "sizeof(globals::GlobalsComponent<bool>)" in main_cpp
