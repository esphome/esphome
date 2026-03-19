"""Shared helpers for component tests."""

from __future__ import annotations

import re

INTERNAL_BIT = 1 << 24


def extract_packed_value(main_cpp: str, var_name: str) -> int:
    """Extract the third (packed) argument from a configure_entity_ call."""
    pattern = (
        rf"{re.escape(var_name)}->configure_entity_\("
        r'"(?:\\.|[^"\\])*"'
        r",\s*\w+,\s*(\d+)\)"
    )
    match = re.search(pattern, main_cpp)
    assert match, f"configure_entity_ call not found for {var_name}"
    return int(match.group(1))
