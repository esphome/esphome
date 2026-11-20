"""Tests for the function that publishes the mpr121 channel binary sensors."""

from collections.abc import Callable
from pathlib import Path


def test_channels_write_thresholds_and_publish_through_hub_functions(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("hub_function.yaml"))

    # A channel without its own thresholds takes the hub's (16 and 8)
    assert (
        "static void esphome__hub__thresholds_fn() {\n"
        "  hub->write_thresholds(0, 16, 8);\n"
        "  hub->write_thresholds(3, 32, 8);\n"
        "}"
    ) in main_cpp
    assert (
        "static void esphome__hub__channels_fn(uint16_t esphome__arg_touched) {\n"
        "  mpr121::publish_channel(::key0, esphome__arg_touched, 0);\n"
        "  mpr121::publish_channel(::key3, esphome__arg_touched, 3);\n"
        "}"
    ) in main_cpp
    assert "hub->set_thresholds_publisher(esphome__hub__thresholds_fn);" in main_cpp
    assert "hub->set_channels_publisher(esphome__hub__channels_fn);" in main_cpp
    assert "idle_hub->set_thresholds_publisher" not in main_cpp
    assert "idle_hub->set_channels_publisher" not in main_cpp
