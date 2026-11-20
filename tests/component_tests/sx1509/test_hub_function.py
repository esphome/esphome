"""Tests for the function that publishes the sx1509 keypad binary sensors."""

from collections.abc import Callable
from pathlib import Path


def test_keypad_publishes_through_hub_function(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("hub_function.yaml"))

    # The key data has the column bit in the high byte and the row bit in the low byte
    assert (
        "static void esphome__hub__keypad_fn(uint16_t esphome__arg_key_data) {\n"
        "  sx1509::publish_key(::key_r0_c0, esphome__arg_key_data, 0x101);\n"
        "  sx1509::publish_key(::key_r1_c1, esphome__arg_key_data, 0x202);\n"
        "}"
    ) in main_cpp
    assert "hub->set_keypad_publisher(esphome__hub__keypad_fn);" in main_cpp
    assert "idle_hub->set_keypad_publisher" not in main_cpp
