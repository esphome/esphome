"""Codegen tests for the functions the systa_bus children generate on their bus."""

from collections.abc import Callable
from pathlib import Path

import pytest


def test_children_publish_through_generated_functions(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
) -> None:
    main_cpp = generate_main(component_config_path("hub_function.yaml"))

    assert (
        "static void esphome__bus__aqua_fn("
        "std::span<const uint8_t> esphome__arg_message) {\n"
        "  ::tsa->publish_state("
        "systa_bus::aqua_temperature(esphome__arg_message, 4));\n"
        "  ::pump->publish_state(esphome__arg_message[12]);\n"
        "  systa_bus::publish_aqua_error_code(::error_code, esphome__arg_message);\n"
        "}"
    ) in main_cpp
    assert "bus->set_aqua_publisher(esphome__bus__aqua_fn);" in main_cpp

    assert (
        "static void esphome__bus__dump_fn(const char * esphome__arg_log_tag) {\n"
        "  systa_bus::dump_aqua_sensor(esphome__arg_log_tag, ::tsa, "
        "systa_bus::AquaSensor::AQUA_SENSOR_TEMPERATURE_TSA);\n"
        "  systa_bus::dump_aqua_sensor(esphome__arg_log_tag, ::pump, "
        "systa_bus::AquaSensor::AQUA_SENSOR_PUMP_SPEED);\n"
        "  systa_bus::dump_aqua_error_code(esphome__arg_log_tag, ::error_code);\n"
        "}"
    ) in main_cpp
    assert "bus->set_dump_publisher(esphome__bus__dump_fn);" in main_cpp

    # The deprecated platform id and setup_priority keys warn and create nothing
    for key in ("id", "setup_priority"):
        assert (
            f"[systa_bus] '{key}' on the sensor platform has no effect and is "
            "deprecated. Will be removed in 2027.5.0"
        ) in caplog.text
    assert "text_sensor platform" not in caplog.text
    code = [
        line for line in main_cpp.splitlines() if not line.lstrip().startswith("//")
    ]
    assert not any("aqua_sensors" in line for line in code)
