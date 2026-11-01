"""Tests for the generated bq25186 status publishers."""

from collections.abc import Callable
from pathlib import Path
import re

from esphome.components.bq25186 import _state_expression

from .status_datasheet import BINARY_SENSORS, TEXT_SENSORS, field_value, status_strings

PARAMS = "const bq25186::BQ25186StatusRegisters & esphome__arg_status"
BINARY = re.compile(r"::bs_(\w+)->publish_state\((.*)\);")
LAST = re.compile(r"static uint8_t esphome__tx_(\w+)__last = 0xFF;")
TEXT = re.compile(
    r"bq25186::publish_status_text\(::tx_(\w+), esphome__tx_\1__last, (.*), (\d+)\);"
)
# The hand copy of the generated function in the C++ behaviour tests
CPP_TEST = Path(__file__).parents[2] / "components/bq25186/test_bq25186_status.cpp"
CPP_BINARY = re.compile(r"entities->binary\[(\w+)\]\.publish_state\((.*)\);")
CPP_TEXT = re.compile(
    r"publish_status_text\(&entities->text\[(\w+)\], entities->\w+, (.*), (\d+)\);"
)


def _body(main_cpp: str, hub: str) -> list[str]:
    head = f"static void esphome__{hub}__status_fn({PARAMS}) {{\n"
    body = main_cpp.split(head, 1)[1].split("\n}", 1)[0]
    return [line.strip() for line in body.splitlines()]


def _check(expr: str, reg: int, bits: int, expected: list[int]) -> None:
    """Evaluate a generated C++ decode, which is also valid Python, for every register value;
    `expected` is the result for each value of the field `bits`."""
    code = compile(expr, expr, "eval")
    for value in range(256):
        status = [0] * 3
        status[reg] = value
        result = eval(code, {"__builtins__": {}}, {"esphome__arg_status": status})  # noqa: S307
        assert result == expected[field_value(value, bits)], (expr, value)


def test_status_publishers(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Every generated decode matches the datasheet; one function per hub with entities,
    items in config order (charger_a's text block comes first)."""
    main_cpp = generate_main(component_config_path("hub_function.yaml"))
    body = _body(main_cpp, "charger_a")
    n_text = 2 * len(TEXT_SENSORS)
    pairs = zip(body[0:n_text:2], body[1:n_text:2], strict=True)
    text = [(LAST.fullmatch(last), TEXT.fullmatch(call)) for last, call in pairs]
    assert [(m[1], t[1]) for m, t in text] == [(key, key) for key in TEXT_SENSORS]
    strings = status_strings()
    for _, call in text:
        reg, bits, states = TEXT_SENSORS[call[1]]
        # publish_status_text keeps the two low bits
        expected = [strings.index(state) for state in states]
        _check(f"{call[3]} + (({call[2]}) & 0x03)", reg, bits, expected)
    binary = [BINARY.fullmatch(line) for line in body[n_text:]]
    assert [m[1] for m in binary] == list(BINARY_SENSORS)
    for match in binary:
        reg, bits, on_values = BINARY_SENSORS[match[1]]
        _check(f"({match[2]}) != 0", reg, bits, [v in on_values for v in range(4)])
    assert _body(main_cpp, "charger_b") == [
        "static uint8_t esphome__b_ts_status__last = 0xFF;",
        "bq25186::publish_status_text(::b_ts_status, esphome__b_ts_status__last, (esphome__arg_status[1] >> 3), 4);",
    ]
    for hub in ("charger_a", "charger_b"):
        assert f"{hub}->set_status_publisher(esphome__{hub}__status_fn);" in main_cpp
    assert "charger_c->set_status_publisher" not in main_cpp

    # The C++ tests run a copy of this function, with the parameter named plain status; it must decode the same way
    cpp = CPP_TEST.read_text().replace("status[", "esphome__arg_status[")
    assert [(m[1].lower(), m[2]) for m in CPP_BINARY.finditer(cpp)] == [
        (m[1], m[2]) for m in binary
    ]
    assert [(m[1].lower(), m[2], m[3]) for m in CPP_TEXT.finditer(cpp)] == [
        (t[1], t[2], t[3]) for _, t in text
    ]


def test_single_bit_shortcut_needs_a_one_bit_field() -> None:
    """A wider field whose match looks like a single bit keeps the bitmap form."""
    for mask, match in ((0x01, 0b10), (0x01, 0b01), (0x03, 0b10), (0x03, 0b0110)):
        expr = str(_state_expression(0, 2, mask, match))
        for value in range(256):
            status = [value]
            result = eval(expr, {"__builtins__": {}}, {"esphome__arg_status": status})  # noqa: S307
            assert (result != 0) == bool((match >> ((value >> 2) & mask)) & 1), (
                mask,
                match,
                value,
            )
