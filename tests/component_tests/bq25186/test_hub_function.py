"""Tests for the generated bq25186 status publishers."""

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
CPP_TEST = Path(__file__).parents[2] / "components/bq25186/test_bq25186_status.cpp"


def test_status_publishers(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Every decode matches the datasheet; one function per hub with entities, items in config order."""
    main_cpp = generate_main(component_config_path("hub_function.yaml"))
    check_status_function(
        main_cpp,
        "charger_a",
        "bq25186::BQ25186StatusStrings",
        progmem_strings("bq25186"),
        TEXT_SENSORS,
        BINARY_SENSORS,
        CPP_TEST,
    )
    assert hub_function_body(main_cpp, "charger_b") == [
        "static uint8_t esphome__b_ts_status__last = 0xFF;",
        (
            "text_sensor::publish_table_on_change<bq25186::BQ25186StatusStrings, 0x03, "
            "bq25186::BQ25186StatusStrings::LAST_INDEX>(::b_ts_status, "
            "esphome__b_ts_status__last, (esphome__arg_status[1] >> 3), 4);"
        ),
    ]
    check_publishers_set(main_cpp, ["charger_a", "charger_b"], ["charger_c"])
