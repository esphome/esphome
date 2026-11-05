"""Tests for the generated bq25820 status publishers."""

from collections.abc import Callable
from pathlib import Path

from ..hub_function_helpers import (
    check_publishers_set,
    check_status_function,
    hub_function_body,
    progmem_strings,
)
from .status_datasheet import BINARY_SENSORS, TEXT_SENSORS

# The hand copy of the generated function in the C++ behaviour tests
CPP_TEST = Path(__file__).parents[2] / "components/bq25820/test_bq25820_status.cpp"


def test_status_publishers(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Every decode matches the datasheet; one function per hub with entities, items in config order."""
    main_cpp = generate_main(component_config_path("hub_function.yaml"))
    check_status_function(
        main_cpp,
        "charger_a",
        "bq25820::BQ25820StatusStrings",
        progmem_strings("bq25820"),
        TEXT_SENSORS,
        BINARY_SENSORS,
        CPP_TEST,
    )
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
