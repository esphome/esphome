"""Tests for the function that publishes the tca8418 binary sensors."""

from collections.abc import Callable
from pathlib import Path


def test_binary_sensors_publish_through_hub_function(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main("tests/component_tests/tca8418/test_tca8418.yaml")

    assert (
        "static void esphome__keypad__key_fn(uint8_t esphome__arg_key, "
        "bool esphome__arg_pressed) {\n"
        "  tca8418::publish_key(::key_four, esphome__arg_key, 12, esphome__arg_pressed);\n"
        "  tca8418::publish_key(::key_one, esphome__arg_key, 1, esphome__arg_pressed);\n"
        "}"
    ) in main_cpp
    assert "keypad->set_key_publisher(esphome__keypad__key_fn);" in main_cpp
    # A keypad without binary sensors keeps a null publisher
    assert "idle_keypad->set_key_publisher" not in main_cpp
