"""Shared helpers for component tests."""

from __future__ import annotations

import re

from esphome.config import Config
from esphome.const import CONF_ID
from esphome.core import ID
from esphome.types import ConfigType

INTERNAL_BIT = 1 << 24


def extract_packed_value(main_cpp: str, var_name: str) -> int:
    """Extract the packed-fields argument from the entity's configure call.

    Matches both legacy form ``var->configure_entity_(name, hash, packed)`` and the
    combined form ``App.register_<entity>(var, name, hash, packed)``.
    """
    escaped_var = re.escape(var_name)
    name_re = r'ESPHOME_PSTR\("(?:\\.|[^"\\])*"\)'
    legacy_pattern = (
        rf"{escaped_var}->configure_entity_\(" + name_re + r",\s*\w+,\s*(\d+)\)"
    )
    combined_pattern = (
        rf"App\.register_\w+\(\s*{escaped_var}\s*,\s*" + name_re + r",\s*\w+,\s*(\d+)\)"
    )
    match = re.search(combined_pattern, main_cpp) or re.search(legacy_pattern, main_cpp)
    assert match, f"configure call not found for {var_name}"
    return int(match.group(1))


def get_define_value(name: str) -> str | None:
    """Rendered value of a CORE define, or None when absent.

    Values are codegen expressions (IntLiteral); they are compared rendered.
    A value-less define (e.g. USE_BK72XX_BLE) is present but renders as the
    string "None", while an absent define returns the None object — easy to
    conflate in assertions, so use this helper for valued defines only.
    """
    from esphome.core import CORE

    for define in CORE.defines:
        if define.name == name:
            return str(define.value)
    return None


def constructed(main_cpp: str, var: str) -> str:
    """The expression placement-new builds ``var`` with, e.g. ``Foo<bar>()``."""
    match = re.search(rf"\bnew\({var}\) (.*?);\n", main_cpp, re.DOTALL)
    assert match is not None, var
    return match.group(1)


def function_body(main_cpp: str, name: str) -> str:
    """The body of the generated ``static void name(...)``."""
    match = re.search(
        rf"static void {name}\([^;{{]*\) \{{(.*?)\n\}}\n", main_cpp, re.DOTALL
    )
    assert match is not None, name
    return match.group(1)


def actions_body(main_cpp: str, function: str) -> str:
    """The body of the function of inline actions that ``function`` calls."""
    match = re.fullmatch(r"\n  (\w+)\(\);", function_body(main_cpp, function))
    assert match is not None, function
    return function_body(main_cpp, match.group(1))


def delayed_automation(main_cpp: str, function: str) -> str:
    """The Automation that ``function`` triggers, checked to hold a delay."""
    match = re.fullmatch(
        r"\n  ::(\w+)->trigger\(\);", function_body(main_cpp, function)
    )
    assert match is not None, function
    automation = match.group(1)
    assert f"new({automation}) Automation<>();" in main_cpp
    assert re.search(rf"{automation}->add_action\(delayaction_id\w*\);", main_cpp)
    return automation


def uart_full_config(
    uarts: list[ConfigType] | None = None, **domains: list[ConfigType]
) -> Config:
    """A full config declaring uart_0 and uart_1 (or uarts), as the ID pass leaves it."""
    uarts = uarts or [{CONF_ID: ID("uart_0")}, {CONF_ID: ID("uart_1")}]
    full = Config()
    full["uart"] = uarts
    for index, uart_conf in enumerate(uarts):
        full.declare_ids.append((uart_conf[CONF_ID], ["uart", index, CONF_ID]))
    full.update(domains)
    return full
