"""Tests for how climate_ir resolves the HEAT_COOL mode during code generation.

HEAT_COOL switches between heating and cooling, so it is only advertised when the
device supports both. That default is computed here rather than in C++, so the
generated call is what these tests check. The `supports_heat_cool` key overrides
the default in both directions.
"""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
import re

import pytest


def _emitted_value(main_cpp: str) -> str:
    """Return the argument of the generated set_supports_heat_cool() call."""
    match = re.search(r"set_supports_heat_cool\((true|false)\)", main_cpp)
    assert match, "set_supports_heat_cool() call not found"
    return match.group(1)


@pytest.mark.parametrize(
    ("config", "expected"),
    [
        ("heat_and_cool.yaml", "true"),
        ("cool_only.yaml", "false"),
        ("heat_only.yaml", "false"),
        ("neither.yaml", "false"),
    ],
)
def test_default_requires_heat_and_cool(
    config: str,
    expected: str,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Without the key, HEAT_COOL follows supports_heat and supports_cool."""
    main_cpp = generate_main(component_config_path(config))
    assert _emitted_value(main_cpp) == expected


@pytest.mark.parametrize(
    ("config", "expected"),
    [
        ("cool_only_override_on.yaml", "true"),
        ("heat_and_cool_override_off.yaml", "false"),
    ],
)
def test_explicit_key_overrides_default(
    config: str,
    expected: str,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A cool-only unit can still offer HEAT_COOL, and a heat+cool unit can drop it."""
    main_cpp = generate_main(component_config_path(config))
    assert _emitted_value(main_cpp) == expected
