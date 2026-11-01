"""Codegen tests for the functions the mk2pvrouter children generate on their hub."""

from collections.abc import Callable
from pathlib import Path


def test_children_publish_through_generated_functions(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("mk2pvrouter.yaml"))

    # Tags are upper-cased and voltage gets the x0.01 correction
    assert (
        "static void esphome__router__value_fn("
        "const char * esphome__arg_tag, const char * esphome__arg_val) {\n"
        '  mk2pvrouter::publish_sensor_value(::power, "P", false, '
        "esphome__arg_tag, esphome__arg_val);\n"
        '  mk2pvrouter::publish_sensor_value(::voltage, "V1", true, '
        "esphome__arg_tag, esphome__arg_val);\n"
    ) in main_cpp
    assert (
        'mk2pvrouter::publish_binary_sensor_value(::relay, "R2", '
        "esphome__arg_tag, esphome__arg_val);\n"
    ) in main_cpp
    assert (
        'mk2pvrouter::publish_text_sensor_value(::status, "S_MC", '
        "esphome__arg_tag, esphome__arg_val);\n"
    ) in main_cpp
    assert "router->set_value_fn(esphome__router__value_fn);" in main_cpp

    assert (
        "static void esphome__router__dump_fn() {\n"
        '  mk2pvrouter::dump_sensor(::power, "P");\n'
    ) in main_cpp
    assert "router->set_dump_fn(esphome__router__dump_fn);" in main_cpp
