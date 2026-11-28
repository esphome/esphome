"""Tests for the function that publishes the ttp229_lsf channel binary sensors."""

from collections.abc import Callable
from pathlib import Path


def test_channels_publish_through_hub_function(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("hub_function.yaml"))

    assert (
        "static void esphome__hub__channels_fn(uint16_t esphome__arg_touched) {\n"
        "  ttp229_lsf::publish_channel(::key0, esphome__arg_touched, 0);\n"
        "  ttp229_lsf::publish_channel(::key15, esphome__arg_touched, 15);\n"
        "}"
    ) in main_cpp
    assert "hub->set_channels_publisher(esphome__hub__channels_fn);" in main_cpp
    assert "idle_hub->set_channels_publisher" not in main_cpp
