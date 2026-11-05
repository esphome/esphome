"""Checks for the status functions that register hubs generate with cg.HubFunction.

The configs name text sensors ``tx_<key>`` and binary sensors ``bs_<key>``; each hub's C++ tests run a
hand copy of the generated function (entities in ``entities->text`` and ``entities->binary``, the
parameter named ``status``), which must decode the same.
"""

from collections.abc import Mapping
from pathlib import Path
import re

COMPONENTS = Path(__file__).parents[2] / "esphome" / "components"
STATUS = "esphome__arg_status"
LAST = re.compile(r"static uint8_t esphome__tx_(\w+)__last = 0xFF;")
BINARY = re.compile(r"::bs_(\w+)->publish_state\((.*)\);")


def hub_function_body(main_cpp: str, hub: str, fn: str = "status") -> list[str]:
    """The statements of the generated ``esphome__<hub>__<fn>_fn``."""
    head = f"static void esphome__{hub}__{fn}_fn("
    # The last match is the definition; a prototype may come first
    body = main_cpp.rsplit(head, 1)[1].split(") {\n", 1)[1].split("\n}", 1)[0]
    return [line.strip() for line in body.splitlines()]


def field_value(value: int, bits: int) -> int:
    """The field `bits` of the register `value`, shifted down."""
    return (value & bits) >> ((bits & -bits).bit_length() - 1)


def progmem_strings(component: str) -> list[str]:
    """The strings of the component's PROGMEM_STRING_TABLE, in table order."""
    for path in sorted((COMPONENTS / component).glob("*.[ch]*")):
        if match := re.search(
            r"PROGMEM_STRING_TABLE\(\w+,(.*?)\);",
            path.read_text(encoding="utf-8"),
            re.DOTALL,
        ):
            return re.findall(r'"([^"]*)"', match[1])
    raise AssertionError(f"no PROGMEM_STRING_TABLE in the {component} component")


def _eval(expr: str, reg: int, value: int) -> int:
    # A generated C++ decode is also valid Python
    status = [0] * 16
    status[reg] = value
    return eval(expr, {"__builtins__": {}}, {STATUS: status})  # noqa: S307


def _flat(code: str) -> str:
    return re.sub(r"\s+", "", code)


def _text_fields(match: re.Match[str]) -> tuple[str, ...]:
    return (match["key"].lower(), *match.group("value", "mask", "first", "fallback"))


def check_status_function(
    main_cpp: str,
    hub: str,
    *,
    table: str,
    text_sensors: Mapping[str, tuple[int, int, tuple[str, ...]]],
    binary_sensors: Mapping[str, tuple[int, int, frozenset[int]]],
    cpp_test: Path,
) -> None:
    """Every decode in the function of `hub` matches the datasheet tables (register offset, field bits,
    then the string per field value or the field values that read as on), text sensors first, in table order;
    `table` is the component's qualified PROGMEM_STRING_TABLE, and the C++ copy in `cpp_test` decodes the same."""
    strings = progmem_strings(table.split("::", 1)[0])
    # Matched without whitespace, as clang-format wraps the C++ copy
    text_call = re.compile(
        re.escape(f"text_sensor::publish_table_on_change<{table},")
        + r"(?P<mask>0x\w+),(?P<fallback>[^>]+)>\((?:::tx_|&entities->text\[)(?P<key>\w+)\]?,"
        r"[\w>-]+,(?P<value>.*?),(?P<first>\d+)\);"
    )
    body = hub_function_body(main_cpp, hub)
    n_text = 2 * len(text_sensors)
    pairs = zip(body[0:n_text:2], body[1:n_text:2], strict=True)
    text = [
        (LAST.fullmatch(last), text_call.fullmatch(_flat(line))) for last, line in pairs
    ]
    assert [(m[1], t["key"]) for m, t in text] == [(key, key) for key in text_sensors]
    for _, match in text:
        key, value_expr, mask, first, fallback = _text_fields(match)
        reg, bits, states = text_sensors[key]
        fallback_index = (
            len(strings) - 1 if fallback == f"{table}::LAST_INDEX" else int(fallback)
        )
        for value in range(256):
            index = int(first) + (_eval(value_expr, reg, value) & int(mask, 16))
            string = strings[index] if index < len(strings) else strings[fallback_index]
            assert string == states[field_value(value, bits)], (key, value)
    binary = [BINARY.fullmatch(_flat(line)) for line in body[n_text:]]
    assert [m[1] for m in binary] == list(binary_sensors)
    for match in binary:
        reg, bits, on_values = binary_sensors[match[1]]
        for value in range(256):
            on = _eval(match[2], reg, value) != 0
            assert on == (field_value(value, bits) in on_values), (match[1], value)

    cpp = _flat(cpp_test.read_text(encoding="utf-8")).replace("status[", f"{STATUS}[")
    cpp_binary = re.compile(r"entities->binary\[(\w+)\]\.publish_state\((.*?)\);")
    assert [(m[1].lower(), m[2]) for m in cpp_binary.finditer(cpp)] == [
        m.groups() for m in binary
    ]
    assert [_text_fields(m) for m in text_call.finditer(cpp)] == [
        _text_fields(t) for _, t in text
    ]


def check_publishers_set(
    main_cpp: str, set_on: list[str], unset_on: list[str], fn: str = "status"
) -> None:
    """Only the hubs in `set_on` got a generated `fn` function."""
    for hub in set_on:
        assert f"{hub}->set_{fn}_publisher(esphome__{hub}__{fn}_fn);" in main_cpp
    for hub in unset_on:
        assert f"{hub}->set_{fn}_publisher" not in main_cpp
