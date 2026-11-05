"""Tests for the generated bq25820 status publishers."""

from collections.abc import Callable
from pathlib import Path

from ..hub_function_helpers import (
    check_binary_decodes,
    check_cpp_copy_matches,
    check_publishers_set,
    check_text_decodes,
    hub_function_body,
)
from .status_datasheet import BINARY_SENSORS, TEXT_SENSORS

# The hand copy of the generated function in the C++ behaviour tests
CPP_TEST = Path(__file__).parents[2] / "components/bq25820/test_bq25820_status.cpp"
TABLE = "bq25820::BQ25820StatusStrings"


def test_text_sensors_decode_the_datasheet(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("hub_function.yaml"))
    check_text_decodes(main_cpp, "charger_a", table=TABLE, text_sensors=TEXT_SENSORS)


def test_binary_sensors_decode_the_datasheet(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("hub_function.yaml"))
    check_binary_decodes(main_cpp, "charger_a", binary_sensors=BINARY_SENSORS)


def test_cpp_copy_matches_the_generated_function(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("hub_function.yaml"))
    check_cpp_copy_matches(main_cpp, "charger_a", table=TABLE, cpp_test=CPP_TEST)


def test_status_publishers(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """One function per hub with entities, items in config order."""
    main_cpp = generate_main(component_config_path("hub_function.yaml"))
    assert hub_function_body(main_cpp, "charger_b") == [
        "static uint8_t esphome__b_temp_status__last = 0xFF;",
        (
            "text_sensor::publish_table_on_change<bq25820::BQ25820StatusStrings, 0x07, 5>("
            "::b_temp_status, esphome__b_temp_status__last, (esphome__arg_status[1] >> 4), 8);"
        ),
    ]
    check_publishers_set(main_cpp, ["charger_a", "charger_b"], ["charger_c"])


def test_dump_status_lists_every_entity(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The hub's dump_config logs each status entity once, in config order."""
    main_cpp = generate_main(component_config_path("hub_function.yaml"))
    ids = [f"tx_{key}" for key in TEXT_SENSORS] + [
        f"bs_{key}" for key in BINARY_SENSORS
    ]

    def log(id_: str) -> str:
        kind = "text_sensor" if id_.startswith(("tx_", "b_")) else "binary_sensor"
        label = "Text Sensor" if kind == "text_sensor" else "Binary Sensor"
        return (
            f'{kind}::log_{kind}(esphome__arg_tag, "  ", '
            f'LOG_STR_LITERAL("{label}"), ::{id_});'
        )

    assert hub_function_body(main_cpp, "charger_a", "dump") == [log(id_) for id_ in ids]
    assert hub_function_body(main_cpp, "charger_b", "dump") == [log("b_temp_status")]
    check_publishers_set(main_cpp, ["charger_a", "charger_b"], ["charger_c"], "dump")
