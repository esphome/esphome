"""Tests for esphome.expression module."""

import pytest

from esphome.expression import (
    SUBSTITUTION_VARIABLE_PROG,
    has_jinja,
    has_substitution_or_expression,
)


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("$name", True),
        ("${name}", True),
        ("plain text", False),
        ("price-100$", False),
        ("no expressions here", False),
        ("${name + 1}", False),  # Jinja expression, not a simple substitution
    ],
)
def test_substitution_variable_prog(value: str, expected: bool) -> None:
    """SUBSTITUTION_VARIABLE_PROG matches $name and ${name} patterns."""
    assert (SUBSTITUTION_VARIABLE_PROG.search(value) is not None) == expected


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("${expr}", True),
        ("${a + b}", True),
        ("<% if true %>yes<% endif %>", True),
        ("$name", False),
        ("plain text", False),
    ],
)
def test_has_jinja(value: str, expected: bool) -> None:
    """has_jinja detects Jinja expressions and block syntax."""
    assert has_jinja(value) == expected


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        # Substitution variables
        ("$name", True),
        ("${name}", True),
        # Jinja expressions
        ("${a + b}", True),
        ("<% block %>", True),
        # Combined
        ("$name and ${expr}", True),
        # No expressions
        ("plain text", False),
        ("path/to/file.yaml", False),
        ("price-100$", False),
        ("price-100$.yaml", False),
    ],
)
def test_has_substitution_or_expression(value: str, expected: bool) -> None:
    """has_substitution_or_expression detects all substitution and Jinja patterns."""
    assert has_substitution_or_expression(value) == expected
