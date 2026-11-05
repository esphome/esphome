"""Tests for the function that publishes the tm1638 key binary sensors."""

from collections.abc import Callable
from pathlib import Path

from ..hub_function_helpers import check_publishers_set


def test_keys_publish_through_hub_function(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("hub_function.yaml"))

    assert (
        "static void esphome__panel__keys_fn(uint8_t esphome__arg_keys) {\n"
        "  tm1638::publish_key(::button0, esphome__arg_keys, 0);\n"
        "  tm1638::publish_key(::button7, esphome__arg_keys, 7);\n"
        "}"
    ) in main_cpp
    # A panel without keys keeps a null publisher, so its loop reads nothing
    check_publishers_set(main_cpp, ["panel"], ["idle_panel"], "keys")
