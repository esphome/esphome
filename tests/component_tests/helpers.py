"""Shared helpers for component tests."""

from __future__ import annotations

import re

INTERNAL_BIT = 1 << 24


def extract_packed_value(main_cpp: str, var_name: str) -> int:
    """Extract the packed-fields argument from the entity's configure call.

    Matches both legacy form ``var->configure_entity_(name, hash, packed)`` and the
    combined form ``App.register_<entity>(var, name, hash, packed)``.
    """
    escaped_var = re.escape(var_name)
    legacy_pattern = (
        rf"{escaped_var}->configure_entity_\("
        r'"(?:\\.|[^"\\])*"'
        r",\s*\w+,\s*(\d+)\)"
    )
    combined_pattern = (
        rf"App\.register_\w+\(\s*{escaped_var}\s*,\s*"
        r'"(?:\\.|[^"\\])*"'
        r",\s*\w+,\s*(\d+)\)"
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
