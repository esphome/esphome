"""Codegen tests for the functions the systa_bus children generate on their bus."""

from collections.abc import Callable
from pathlib import Path


def test_children_publish_through_generated_functions(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("systa_bus.yaml"))

    assert (
        "static void esphome__bus__aqua_sensor_data_fn("
        "std::span<const uint8_t> esphome__arg_message) {\n"
        "  ::tsa->publish_state("
        "systa_bus::aqua_temperature(esphome__arg_message, 4));\n"
        "  ::pump->publish_state(esphome__arg_message[12]);\n"
        "  systa_bus::publish_aqua_error_code(::error_code, esphome__arg_message);\n"
        "}"
    ) in main_cpp
    assert (
        "bus->set_aqua_sensor_data_fn(esphome__bus__aqua_sensor_data_fn);" in main_cpp
    )

    assert (
        "static void esphome__bus__dump_fn(const char * esphome__arg_tag) {\n"
        '  sensor::log_sensor(esphome__arg_tag, "  ", '
        'LOG_STR_LITERAL("Temperature TSA"), ::tsa);\n'
    ) in main_cpp
    assert "bus->set_dump_fn(esphome__bus__dump_fn);" in main_cpp
