"""Tests for the generated bq25820 status publishers."""

from collections.abc import Callable
from pathlib import Path
import re

from esphome.components.bq25820.binary_sensor import _state_expression

from .status_datasheet import BINARY_SENSORS, TEXT_SENSORS, field_value, status_strings

BINARY = re.compile(r"::bs_(\w+)->publish_state\((.*)\);")
LAST = re.compile(r"static uint8_t esphome__tx_(\w+)__last = 0xFF;")
TEXT = re.compile(
    r"bq25820::publish_status_text\(::tx_(\w+), esphome__tx_\1__last, (.*), (\d+)\);"
)
# The hand copy of the generated function in the C++ behaviour tests
CPP_TEST = Path(__file__).parents[2] / "components/bq25820/test_bq25820_status.cpp"
CPP_BINARY = re.compile(r"entities->binary\[(\w+)\]\.publish_state\((.*)\);")
CPP_TEXT = re.compile(
    r"publish_status_text\(&entities->text\[(\w+)\], entities->\w+, (.*), (\d+)\);"
)


def _body(main_cpp: str, hub: str, fn: str = "status") -> list[str]:
    head = f"static void esphome__{hub}__{fn}_fn("
    # The last match is the definition; a prototype comes first
    body = main_cpp.rsplit(head, 1)[1].split(") {\n", 1)[1].split("\n}", 1)[0]
    return [line.strip() for line in body.splitlines()]


def _check(
    expr: str,
    reg: int,
    bits: int,
    expected: list[object] | tuple[object, ...],
    decode: Callable[[int], object] = lambda result: result,
) -> None:
    """Evaluate a generated C++ decode, which is also valid Python, for every register value;
    `expected` is the decoded result for each value of the field `bits`."""
    code = compile(expr, expr, "eval")
    for value in range(256):
        status = [0] * 4
        status[reg] = value
        result = eval(code, {"__builtins__": {}}, {"esphome__arg_status": status})  # noqa: S307
        assert decode(result) == expected[field_value(value, bits)], (expr, value)


def test_status_publishers(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Every decode matches the datasheet; one function per hub with entities, items in config order."""
    main_cpp = generate_main(component_config_path("hub_function.yaml"))
    body = _body(main_cpp, "charger_a")
    n_text = 2 * len(TEXT_SENSORS)
    pairs = zip(body[0:n_text:2], body[1:n_text:2], strict=True)
    text = [(LAST.fullmatch(last), TEXT.fullmatch(call)) for last, call in pairs]
    assert [(m[1], t[1]) for m, t in text] == [(key, key) for key in TEXT_SENSORS]
    strings = status_strings()

    def string_at(index: int) -> str:
        # publish_status_text falls back to "Unknown" past the end of the table
        return strings[index] if index < len(strings) else strings[5]

    for _, call in text:
        reg, bits, states = TEXT_SENSORS[call[1]]
        # publish_status_text keeps the three low bits
        _check(f"{call[3]} + (({call[2]}) & 0x07)", reg, bits, states, string_at)
    binary = [BINARY.fullmatch(line) for line in body[n_text:]]
    assert [m[1] for m in binary] == list(BINARY_SENSORS)
    for match in binary:
        reg, bits, on_values = BINARY_SENSORS[match[1]]
        _check(f"({match[2]}) != 0", reg, bits, [v in on_values for v in range(8)])
    assert _body(main_cpp, "charger_b") == [
        "static uint8_t esphome__b_temp_status__last = 0xFF;",
        "bq25820::publish_status_text(::b_temp_status, esphome__b_temp_status__last, (esphome__arg_status[1] >> 4), 8);",
    ]
    for hub in ("charger_a", "charger_b"):
        assert f"{hub}->set_status_publisher(esphome__{hub}__status_fn);" in main_cpp
    assert "charger_c->set_status_publisher" not in main_cpp

    # The C++ tests run a copy of this function (parameter named status); it must decode the same
    cpp = CPP_TEST.read_text().replace("status[", "esphome__arg_status[")
    assert [(m[1].lower(), m[2]) for m in CPP_BINARY.finditer(cpp)] == [
        (m[1], m[2]) for m in binary
    ]
    assert [(m[1].lower(), m[2], m[3]) for m in CPP_TEXT.finditer(cpp)] == [
        (t[1], t[2], t[3]) for _, t in text
    ]


def test_single_bit_shortcut_needs_a_one_bit_field() -> None:
    """A wider field whose match looks like a single bit keeps the bitmap form."""
    for mask, match in ((0x01, 0b10), (0x01, 0b01), (0x07, 0b10), (0x07, 0b01111110)):
        expr = str(_state_expression(0, 2, mask, match))
        for value in range(256):
            status = [value]
            result = eval(expr, {"__builtins__": {}}, {"esphome__arg_status": status})  # noqa: S307
            assert (result != 0) == bool((match >> ((value >> 2) & mask)) & 1), (
                mask,
                match,
                value,
            )


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

    assert _body(main_cpp, "charger_a", "dump") == [log(id_) for id_ in ids]
    assert _body(main_cpp, "charger_b", "dump") == [log("b_temp_status")]
    for hub in ("charger_a", "charger_b"):
        assert f"{hub}->set_dump_publisher(esphome__{hub}__dump_fn);" in main_cpp
    assert "charger_c->set_dump_publisher" not in main_cpp
