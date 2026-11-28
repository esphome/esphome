"""Tests for the function that publishes the cap1188 channel binary sensors."""

from collections.abc import Callable
from pathlib import Path


def test_channels_publish_through_hub_function(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("hub_function.yaml"))

    assert (
        "static void esphome__hub__channels_fn(uint8_t esphome__arg_touched) {\n"
        "  cap1188::publish_channel(::key0, esphome__arg_touched, 0);\n"
        "  cap1188::publish_channel(::key7, esphome__arg_touched, 7);\n"
        "}"
    ) in main_cpp
    assert "hub->set_channels_publisher(esphome__hub__channels_fn);" in main_cpp
    # A hub without channels keeps a null publisher
    assert "idle_hub->set_channels_publisher" not in main_cpp
