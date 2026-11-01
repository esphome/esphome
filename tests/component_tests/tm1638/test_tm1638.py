"""Tests for the function that publishes the tm1638 key binary sensors."""

from collections.abc import Callable
from pathlib import Path


def test_keys_publish_through_hub_function(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main("tests/component_tests/tm1638/test_tm1638.yaml")

    assert (
        "static void esphome__panel__keys_fn(uint8_t esphome__arg_keys) {\n"
        "  tm1638::publish_key(::button0, esphome__arg_keys, 0);\n"
        "  tm1638::publish_key(::button7, esphome__arg_keys, 7);\n"
        "}"
    ) in main_cpp
    assert "panel->set_keys_publisher(esphome__panel__keys_fn);" in main_cpp
    # A panel without keys keeps a null publisher, so its loop reads nothing
    assert "idle_panel->set_keys_publisher" not in main_cpp
