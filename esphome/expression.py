"""Helpers for detecting substitution variables and Jinja expressions."""

import re

from esphome.const import VALID_SUBSTITUTIONS_CHARACTERS

SUBSTITUTION_VARIABLE_PROG = re.compile(
    rf"\$([{VALID_SUBSTITUTIONS_CHARACTERS}]+|\{{[{VALID_SUBSTITUTIONS_CHARACTERS}]*\}})"
)

_JINJA_RE = re.compile(
    r"<%.+?%>"  # Block: <% ... %>
    r"|\$\{[^}]+\}",  # Braced: ${ ... }
    flags=re.MULTILINE,
)


def has_jinja(value: str) -> bool:
    """Check if a string contains Jinja expressions."""
    return _JINJA_RE.search(value) is not None


def has_substitution_or_expression(value: str) -> bool:
    """Check if a string contains substitution variables ($name, ${name}) or Jinja expressions."""
    return SUBSTITUTION_VARIABLE_PROG.search(value) is not None or has_jinja(value)
