"""Unit tests for the ESP_LOG call scanner in script/ci-custom.py."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys

import pytest

SCRIPT_DIR = (Path(__file__).parent / ".." / ".." / "script").resolve()
sys.path.insert(0, str(SCRIPT_DIR))

_spec = importlib.util.spec_from_file_location("ci_custom", SCRIPT_DIR / "ci-custom.py")
ci_custom = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ci_custom)


def _calls(content: str) -> list[str | None]:
    return [text for _, text in ci_custom._iter_log_calls(content)]


def _ternary_errors(content: str) -> list[tuple[int, int]]:
    errs = ci_custom.lint_log_no_bare_literal_ternary(Path("x.cpp"), content)
    return [(line, col) for line, col, _ in errs]


@pytest.mark.parametrize(
    "content",
    [
        'ESP_LOGD(TAG, "a ) b ( c; d")',
        'ESP_LOGD(TAG, "quote \\" inside")',
        "ESP_LOGD(TAG, \"%s\", format_hex_pretty(x, '-', false).c_str())",
        "ESP_LOGD(TAG, \"%c%c\", '(', ')')",
        "ESP_LOGD(TAG, \"%d\", 1'000'000)",
        'ESP_LOGD(TAG,  // it\'s a comment with ) and (\n         "x")',
        'ESP_LOGD(TAG, /* :) */ "x")',
    ],
)
def test_iter_log_calls_spans_whole_call(content: str) -> None:
    calls = _calls(content + ";\nint other = (1);")
    assert calls == [content]


def test_iter_log_calls_reports_unbalanced_call() -> None:
    content = 'ESP_LOGD(TAG, "x";\nvoid f();'
    assert _calls(content) == [None]
    errs = ci_custom.lint_log_no_bare_literal_ternary(Path("x.cpp"), content)
    assert len(errs) == 1
    assert errs[0][:2] == (1, 1)
    assert "no matching closing parenthesis" in errs[0][2]


@pytest.mark.parametrize(
    ("content", "expected"),
    [
        ('ESP_LOGD(TAG, "%s", x ? "on" : "off");', [(1, 25), (1, 32)]),
        (
            'ESP_LOGD(TAG, "%s", x ? LOG_STR_LITERAL("on") : LOG_STR_LITERAL("off"));',
            [],
        ),
        ('ESP_LOGD(TAG, "%s", x ? LOG_STR_LITERAL("on") : "off");', [(1, 49)]),
        ('ESP_LOGD(TAG, "%s", x ? "on" : "");', [(1, 25)]),
        (
            'ESP_LOGD(TAG, "%s",\n         x ? "yes"\n           : "no");',
            [(2, 14), (3, 14)],
        ),
        ("ESP_LOGD(TAG, \"%c\", x ? '1' : '0');", []),
        ('ESP_LOGD(TAG, "a ? b : c %s", x ? "on" : "off");', [(1, 35), (1, 42)]),
        ('ESP_LOGD(TAG, "x:" "y %s", p);', []),
        ('ESP_LOGD(TAG, "%s", x ? "on" : "off");  // NOLINT', []),
        ('ESP_LOGD(TAG, "%s",\n         x ? "yes"\n           : "no");  // NOLINT', []),
    ],
)
def test_ternary_literal_detection(
    content: str, expected: list[tuple[int, int]]
) -> None:
    assert _ternary_errors(content) == expected


def test_ternary_error_message_names_the_literal() -> None:
    errs = ci_custom.lint_log_no_bare_literal_ternary(
        Path("x.cpp"), 'ESP_LOGD(TAG, "%s", x ? "enabled" : LOG_STR_LITERAL("off"));'
    )
    assert len(errs) == 1
    assert 'LOG_STR_LITERAL("enabled")' in errs[0][2]
