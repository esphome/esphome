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
LAST_DECLARATION = re.compile(r"static uint8_t esphome__tx_(\w+)__last = 0xFF;")
BINARY_PUBLISH = re.compile(r"::bs_(\w+)->publish_state\((.*)\);")
COPIED_BINARY_PUBLISH = re.compile(
    r"entities->binary\[(\w+)\]\.publish_state\((.*?)\);"
)


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


def _decode(expr: str, register: int, value: int) -> int:
    """Run a generated C++ decode, which is also valid Python, with ``register`` holding ``value``."""
    registers = [0] * 16  # more registers than any hub reads
    registers[register] = value
    return eval(expr, {"__builtins__": {}}, {STATUS: registers})  # noqa: S307


def _flat(code: str) -> str:
    # Matched without whitespace, as clang-format wraps the C++ copy
    return re.sub(r"\s+", "", code)


def _text_publish(table: str) -> re.Pattern[str]:
    """A flattened ``publish_table_on_change`` call, generated (``::tx_<key>``) or copied (``&entities->text[KEY]``)."""
    return re.compile(
        re.escape(f"text_sensor::publish_table_on_change<{table},")
        + r"(?P<mask>0x\w+),(?P<fallback>[^>]+)>\((?:::tx_|&entities->text\[)(?P<key>\w+)\]?,"
        r"[\w>-]+,(?P<value>.*?),(?P<first>\d+)\);"
    )


def _text_fields(match: re.Match[str]) -> tuple[str, ...]:
    return (match["key"].lower(), *match.group("value", "mask", "first", "fallback"))


def check_text_decodes(
    main_cpp: str,
    hub: str,
    *,
    table: str,
    text_sensors: Mapping[str, tuple[int, int, tuple[str, ...]]],
) -> None:
    """The function of ``hub`` starts with a last-value declaration and a publish per text sensor, in table
    order, and each publishes the datasheet string (``text_sensors``: register, field bits, string per value)."""
    strings = progmem_strings(table.split("::", 1)[0])
    lines = hub_function_body(main_cpp, hub)[: 2 * len(text_sensors)]
    declared = [LAST_DECLARATION.fullmatch(line)[1] for line in lines[0::2]]
    calls = [_text_publish(table).fullmatch(_flat(line)) for line in lines[1::2]]
    assert declared == [call["key"] for call in calls] == list(text_sensors)
    for call in calls:
        key, value_expr, mask, first, fallback = _text_fields(call)
        register, bits, states = text_sensors[key]
        last_index = len(strings) - 1
        fallback_index = (
            last_index if fallback == f"{table}::LAST_INDEX" else int(fallback)
        )
        for value in range(256):
            index = int(first) + (_decode(value_expr, register, value) & int(mask, 16))
            string = strings[index if index <= last_index else fallback_index]
            assert string == states[field_value(value, bits)], (key, value)


def check_binary_decodes(
    main_cpp: str,
    hub: str,
    *,
    binary_sensors: Mapping[str, tuple[int, int, frozenset[int]]],
) -> None:
    """The function of ``hub`` ends with a publish per binary sensor, in table order, each on for exactly the
    datasheet's field values (``binary_sensors``: register, field bits, field values that read as on)."""
    lines = hub_function_body(main_cpp, hub)[-len(binary_sensors) :]
    publishes = [BINARY_PUBLISH.fullmatch(line) for line in lines]
    assert [publish[1] for publish in publishes] == list(binary_sensors)
    for publish in publishes:
        register, bits, on_values = binary_sensors[publish[1]]
        for value in range(256):
            on = _decode(publish[2], register, value) != 0
            assert on == (field_value(value, bits) in on_values), (publish[1], value)


def check_cpp_copy_matches(
    main_cpp: str, hub: str, *, table: str, cpp_test: Path
) -> None:
    """The hand copy of the function of ``hub`` in ``cpp_test`` (parameter ``status``) has the same publishes."""
    generated = [_flat(line) for line in hub_function_body(main_cpp, hub)]
    copy = _flat(cpp_test.read_text(encoding="utf-8")).replace("status[", f"{STATUS}[")
    text_publish = _text_publish(table)
    assert [_text_fields(m) for m in text_publish.finditer(copy)] == [
        _text_fields(m) for line in generated if (m := text_publish.fullmatch(line))
    ]
    assert [(m[1].lower(), m[2]) for m in COPIED_BINARY_PUBLISH.finditer(copy)] == [
        m.groups() for line in generated if (m := BINARY_PUBLISH.fullmatch(line))
    ]


def check_publishers_set(
    main_cpp: str, set_on: list[str], unset_on: list[str], fn: str = "status"
) -> None:
    """Only the hubs in `set_on` got a generated `fn` function."""
    for hub in set_on:
        assert f"{hub}->set_{fn}_publisher(esphome__{hub}__{fn}_fn);" in main_cpp
    for hub in unset_on:
        assert f"{hub}->set_{fn}_publisher" not in main_cpp
