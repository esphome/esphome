"""A constant select.set option of a select with a fixed option list becomes its index."""

from collections.abc import Callable
from pathlib import Path


def _call(select_id: str, statement: str) -> str:
    return f"auto apply_call = ::{select_id}->make_call();\n  apply_call.{statement};"


def test_select_set_constant_uses_index(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("select_set.yaml"))

    # Generated before the select, and from the select's own on_value
    assert _call("fan", "set_index(2)") in main_cpp
    assert _call("fan", "set_index(1)") in main_cpp
    assert 'set_option("high"' not in main_cpp
    # Not an option: the lookup by text still logs it at run time
    assert _call("fan", 'set_option("turbo", 5)') in main_cpp
    # A lambda's value is only known at run time
    assert _call("fan", 'set_option("low")') in main_cpp
    # A copy select gets its options at run time
    assert _call("fan_copy", 'set_option("medium", 6)') in main_cpp


def test_select_set_lvgl_keeps_lookup_by_text(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Lvgl dropdown options can be replaced at run time, so the index is not fixed."""
    main_cpp = generate_main(component_config_path("select_set_lvgl.yaml"))
    assert _call("lv_fan", 'set_option("high", 4)') in main_cpp


def test_select_set_hub_select_uses_index(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("select_set_hub.yaml"))
    assert _call("power", "set_index(3)") in main_cpp
