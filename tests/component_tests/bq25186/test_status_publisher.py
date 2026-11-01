"""Tests for the generated bq25186 status publishers."""

from collections.abc import Callable
import re

from .status_datasheet import BINARY_SENSORS, TEXT_SENSORS, field_value, status_strings

PARAMS = "const bq25186::BQ25186StatusRegisters & status"


def _body(main_cpp: str, hub: str) -> list[str]:
    head = f"static void esphome__{hub}__status_fn({PARAMS}) {{\n"
    body = main_cpp.split(head, 1)[1].split("\n}", 1)[0]
    return [line.strip() for line in body.splitlines()]


def test_status_publishers(generate_main: Callable[[str], str]) -> None:
    """One function per hub with entities: binary sensors in block and field order, then text sensors."""
    main_cpp = generate_main("tests/component_tests/bq25186/test_bq25186_status.yaml")
    assert _body(main_cpp, "charger_a") == [
        "::a_ts->publish_state((status[0] >> 7) & 0x01);",
        "::a_charging->publish_state((0b0110 >> ((status[0] >> 5) & 0x03)) & 1);",
        "::a_battery_ocp_flag->publish_state((status[2] >> 0) & 0x01);",
        "static uint8_t esphome__a_charge_status__last = 0xFF;",
        "bq25186::publish_status_text(::a_charge_status, esphome__a_charge_status__last, status[0] >> 5, 0);",
        "static uint8_t esphome__a_ts_status__last = 0xFF;",
        "bq25186::publish_status_text(::a_ts_status, esphome__a_ts_status__last, status[1] >> 3, 4);",
    ]
    assert _body(main_cpp, "charger_b") == [
        "static uint8_t esphome__b_ts_status__last = 0xFF;",
        "bq25186::publish_status_text(::b_ts_status, esphome__b_ts_status__last, status[1] >> 3, 4);",
    ]
    for hub in ("charger_a", "charger_b"):
        assert f"{hub}->set_status_publisher(esphome__{hub}__status_fn);" in main_cpp
    assert "charger_c->set_status_publisher" not in main_cpp
    assert "esphome__charger_c__status_fn" not in main_cpp


def _decode(expr: str, reg: int, value: int) -> int:
    """Evaluate a generated C++ decode expression, which is also valid Python."""
    status = [0] * 3
    status[reg] = value
    return eval(expr, {"__builtins__": {}}, {"status": status})  # noqa: S307


def test_generated_decode_matches_the_datasheet(
    generate_main: Callable[[str], str],
) -> None:
    """Each sensor's generated decode gives the datasheet result for every register value."""
    main_cpp = generate_main(
        "tests/component_tests/bq25186/test_bq25186_all_status.yaml"
    )
    body = _body(main_cpp, "charger")
    strings = status_strings()
    binary = [
        re.fullmatch(r"::bs_(\w+)->publish_state\((.*)\);", line) for line in body
    ]
    binary = [m for m in binary if m]
    assert [m[1] for m in binary] == list(BINARY_SENSORS)
    for match in binary:
        reg, bits, on_values = BINARY_SENSORS[match[1]]
        for value in range(256):
            expected = field_value(value, bits) in on_values
            assert bool(_decode(match[2], reg, value)) == expected, (match[1], value)
    text = [
        re.fullmatch(
            r"bq25186::publish_status_text\(::tx_(\w+), esphome__tx_\1__last, (.*), (\d+)\);",
            line,
        )
        for line in body
    ]
    text = [m for m in text if m]
    assert [m[1] for m in text] == list(TEXT_SENSORS)
    for match in text:
        reg, bits, states = TEXT_SENSORS[match[1]]
        assert f"static uint8_t esphome__tx_{match[1]}__last = 0xFF;" in body
        for value in range(256):
            # publish_status_text keeps the two low bits
            index = int(match[3]) + (_decode(match[2], reg, value) & 0x03)
            assert strings[index] == states[field_value(value, bits)], (match[1], value)
    assert len(body) == len(binary) + 2 * len(text)
