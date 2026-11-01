"""Tests for the generated bq25186 status publishers."""

from collections.abc import Callable

YAML = "tests/component_tests/bq25186/test_bq25186_status.yaml"
PARAMS = "const bq25186::BQ25186StatusRegisters & status"


def _body(main_cpp: str, hub: str) -> list[str]:
    head = f"static void esphome__{hub}__status_fn({PARAMS}) {{\n"
    body = main_cpp.split(head, 1)[1].split("\n}", 1)[0]
    return [line.strip() for line in body.splitlines()]


def test_binary_sensors_then_text_sensors_in_field_order(
    generate_main: Callable[[str], str],
) -> None:
    """One function per hub: binary sensors in block and field order, then text sensors."""
    main_cpp = generate_main(YAML)
    assert _body(main_cpp, "charger_a") == [
        "::a_ts->publish_state((status[0] >> 7) & 0x01);",
        "::a_charging->publish_state((0b0110 >> ((status[0] >> 5) & 0x03)) & 1);",
        "::a_battery_ocp_flag->publish_state((status[2] >> 0) & 0x01);",
        "static uint8_t esphome__a_charge_status__last = 0xFF;",
        (
            "bq25186::publish_status_text(::a_charge_status, esphome__a_charge_status__last, "
            "(status[0] >> 5) & 0x03, 0);"
        ),
        "static uint8_t esphome__a_ts_status__last = 0xFF;",
        (
            "bq25186::publish_status_text(::a_ts_status, esphome__a_ts_status__last, "
            "(status[1] >> 3) & 0x03, 4);"
        ),
    ]
    assert _body(main_cpp, "charger_b") == [
        "static uint8_t esphome__b_ts_status__last = 0xFF;",
        (
            "bq25186::publish_status_text(::b_ts_status, esphome__b_ts_status__last, "
            "(status[1] >> 3) & 0x03, 4);"
        ),
    ]


def test_only_hubs_with_entities_get_a_publisher(
    generate_main: Callable[[str], str],
) -> None:
    main_cpp = generate_main(YAML)
    assert "charger_a->set_status_publisher(esphome__charger_a__status_fn);" in main_cpp
    assert "charger_b->set_status_publisher(esphome__charger_b__status_fn);" in main_cpp
    assert "charger_c->set_status_publisher" not in main_cpp
    assert "esphome__charger_c__status_fn" not in main_cpp
