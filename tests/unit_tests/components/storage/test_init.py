"""Unit tests for the storage component's config validators."""

from __future__ import annotations

import pytest

from esphome.components.storage import _validate_regex
import esphome.config_validation as cv

# _validate_regex guards the extract: `regex:` step. The runtime compiles the pattern with
# std::regex in the default ECMAScript grammar, and ESPHome builds with -fno-exceptions, so a
# pattern Python's re accepts but std::regex rejects would abort the node at runtime rather than
# raise. The validator hand-rolls a scanner to catch those at config time; these tests pin the
# behaviour it deliberately encodes so a refactor of the scanner has a regression net.


# Valid patterns whose constructs std::regex ECMAScript supports: they must pass through unchanged.
@pytest.mark.parametrize(
    "pattern",
    [
        "abc",
        "^a.*b$",
        "[a-z]+",
        r"\d{3}",
        "(?:abc)",  # non-capturing group
        "(?=abc)",  # lookahead
        "(?!abc)",  # negative lookahead
        "[(?]",  # '(?' inside a character class is a literal, not a group opener
        r"a\(?b",  # the '(' is escaped, so the following '?' is an ordinary quantifier
    ],
)
def test_validate_regex_accepts_supported_patterns(pattern: str) -> None:
    assert _validate_regex(pattern) == pattern


# Patterns Python's re accepts but std::regex ECMAScript rejects -- must be refused at config time.
@pytest.mark.parametrize(
    "pattern",
    [
        "(?P<name>x)",  # named group
        "(?<=x)",  # lookbehind
        "(?i)x",  # inline flag
        r"\Ax",  # \A anchor (std::regex has only ^)
        r"x\Z",  # \Z anchor (std::regex has only $)
        "a*+",  # possessive quantifier
        "a{1,2}+",  # possessive quantifier on a bounded repeat
    ],
)
def test_validate_regex_rejects_unsupported_constructs(pattern: str) -> None:
    with pytest.raises(cv.Invalid):
        _validate_regex(pattern)


# A pattern that is not valid regex at all is caught by the re.compile() guard rather than the
# std::regex scanner; both paths raise cv.Invalid.
def test_validate_regex_rejects_syntactically_invalid_pattern() -> None:
    with pytest.raises(cv.Invalid):
        _validate_regex("a(b")
