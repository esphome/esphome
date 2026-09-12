"""Unit tests for the ESP_LOG-needs-braces lint rule in script/ci-custom.py.

The rule flags an if/else/for/while whose only body is an unbraced ESP_LOG*() call (which becomes an
empty statement -- and a -Wempty-body warning -- once the log level compiles the macro out). These
tests pin the comment/string/raw-string masker, the accepted control-statement shapes, and the
NOLINT escape hatch at both placements a contributor would try.

Also covers the ESP_LOG call scanner (_iter_log_calls) and the bare-literal-ternary lint.
"""

import importlib.util
from pathlib import Path
import sys

import pytest

SCRIPT_DIR = (Path(__file__).parent / ".." / ".." / "script").resolve()
sys.path.insert(0, str(SCRIPT_DIR))
_spec = importlib.util.spec_from_file_location("ci_custom", SCRIPT_DIR / "ci-custom.py")
ci_custom = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ci_custom)

mask = ci_custom._mask_cpp_comments_strings


def _lint(content: str) -> list:
    return ci_custom.lint_esp_log_needs_braces("test.cpp", content)


# --- masker ---


def test_mask_preserves_length_newlines_and_real_parens() -> None:
    src = 'foo("bar") + baz();\nqux();\n'
    masked = mask(src)
    assert len(masked) == len(src)
    assert masked.count("\n") == src.count("\n")
    assert masked.count("(") == src.count("(")  # real parens survive for balancing


def test_mask_blanks_line_and_block_comments() -> None:
    assert "ESP_LOGD" not in mask("a; // if (x) ESP_LOGD(t);\n")
    assert "ESP_LOGD" not in mask("a; /* if (x) ESP_LOGD(t); */ b;\n")


def test_mask_blanks_string_literals() -> None:
    assert "if" not in mask('x = "if (y) ESP_LOGD";\n')


def test_mask_handles_raw_string_without_desync() -> None:
    # A raw string full of quotes/parens must be consumed as one unit; code after it stays intact.
    src = 's.print(R"(<a href="x">)");\nreturn;\n'
    masked = mask(src)
    assert "href" not in masked
    assert "return;" in masked  # not swallowed by a desynced string scan


# --- rule: flags real violations ---


def test_flags_unbraced_if_next_line() -> None:
    assert _lint("if (x)\n  ESP_LOGD(t);\n")


def test_flags_unbraced_same_line() -> None:
    assert _lint("if (x) ESP_LOGW(t);\n")


def test_flags_c_style_for() -> None:
    assert _lint("for (int i = 0; i < n; i++)\n  ESP_LOGD(t, i);\n")


def test_flags_range_for_and_else() -> None:
    assert _lint("for (auto &x : v)\n  ESP_LOGCONFIG(t);\n")
    assert _lint("else\n  ESP_LOGE(t);\n")


def test_flags_for_header_with_nested_call() -> None:
    assert _lint("for (auto it = v.begin(); it != v.end(); ++it)\n  ESP_LOGD(t);\n")


def test_for_header_does_not_reach_into_a_later_statement() -> None:
    # The 'for' header is bounded to its own statement, so it cannot swallow the loop body and latch
    # onto a later ')'. Without that, the '#if' line below is reported as an unbraced body even though
    # the '#' preprocessor check should skip it.
    assert not _lint(
        "for (int i = 0; i < n; i++)\n  arr[i] = 0;\n#if defined(USE_X)\n  ESP_LOGD(t);\n#endif\n"
    )


def test_violation_after_a_for_loop_is_reported_at_its_own_line() -> None:
    errors = _lint(
        "for (int i = 0; i < n; i++)\n  sum += a[i];\nif (verbose)\n  ESP_LOGD(t, sum);\n"
    )
    lines = [line for line, _col, _msg in errors]
    assert lines == [3]  # the 'if', not the 'for' on line 1


def test_flags_lowercase_esph_log_family() -> None:
    # core/log.h defines esph_log_*() alongside ESP_LOG*(); both expand to nothing below their level.
    assert _lint('if (x)\n  esph_log_config(t, "m");\n')
    assert _lint('if (err != ESP_OK)\n  esph_log_e(t, "m");\n')


def test_digit_separator_does_not_disable_the_rest_of_the_file() -> None:
    # A "'" digit separator must not be read as a char-literal opener, which blanked everything after.
    assert _lint("uint32_t x = 1'000;\nif (y)\n  ESP_LOGD(t);\n")


def test_mask_still_blanks_real_char_literals() -> None:
    assert "ESP_LOGD" not in mask("char c = '\"'; // if (x) ESP_LOGD(t);\n")
    assert not _lint("char sep = ';';\nif (x) {\n  ESP_LOGD(t);\n}\n")


def test_flags_multiline_log_body() -> None:
    assert _lint('if (x)\n  ESP_LOGD(t, "%d %d",\n           a, b);\n')


def test_raw_string_before_violation_still_caught() -> None:
    # Regression for the masker desyncing on a raw string and disabling the check for the rest.
    assert _lint('s.print(R"(<a href="x">)");\nif (y)\n  ESP_LOGD(t);\n')


# --- rule: ignores non-violations ---


def test_ignores_braced_body() -> None:
    assert not _lint("if (x) {\n  ESP_LOGD(t);\n}\n")


def test_ignores_commented_out_code() -> None:
    assert not _lint("// if (x) ESP_LOGD(t);\n")


def test_ignores_preprocessor_else() -> None:
    assert not _lint("#else\n  ESP_LOGCONFIG(t);\n#endif\n")


def test_ignores_non_log_body() -> None:
    assert not _lint("if (x)\n  return false;\n")


# --- NOLINT escape hatch, both placements ---


def test_nolint_at_end_of_log_line_suppresses() -> None:
    assert not _lint("if (x)\n  ESP_LOGD(t);  // NOLINT\n")


def test_nolint_on_control_line_suppresses() -> None:
    assert not _lint("if (x)  // NOLINT\n  ESP_LOGD(t);\n")


# --- ESP_LOG call scanner and bare-literal-ternary lint ---


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
        'ESP_LOGD(TAG, "%s", R"(say "hi" :) )")',
        'ESP_LOGD(TAG, "%s", R"x(a)"b)x")',
    ],
)
def test_iter_log_calls_spans_whole_call(content: str) -> None:
    calls = _calls(content + ";\nint other = (1);")
    assert calls == [content]


def test_iter_log_calls_reports_unbalanced_call_once() -> None:
    content = 'ESP_LOGD(TAG, "x";\nvoid f();'
    assert _calls(content) == [None]
    errs = ci_custom.lint_log_multiline_continuation(Path("x.cpp"), content)
    assert len(errs) == 1
    assert errs[0][:2] == (1, 1)
    assert "no matching closing parenthesis" in errs[0][2]
    assert _ternary_errors(content) == []


@pytest.mark.parametrize(
    ("content", "expected"),
    [
        # A ; inside the format string no longer cuts the call short
        ('ESP_LOGD(TAG, "a; b\\nc %s", x);', [(1, 20)]),
        # A \n%s continuation is exempt since %s may expand to leading whitespace
        ('ESP_LOGD(TAG, "a\\n%s", x);', []),
        ('ESP_LOGD(TAG, "a\\n  b");', []),
    ],
)
def test_multiline_continuation_detection(
    content: str, expected: list[tuple[int, int]]
) -> None:
    errs = ci_custom.lint_log_multiline_continuation(Path("x.cpp"), content)
    assert [(line, col) for line, col, _ in errs] == expected


def test_exclusion_list_only_names_components_without_esp8266_tests() -> None:
    root = Path(__file__).parent / ".." / ".."
    for pattern in ci_custom.LOG_LITERAL_LINT_EXCLUDE:
        if not pattern.startswith("esphome/components/"):
            continue
        prefix = pattern.removeprefix("esphome/components/").split("/")[0]
        comps = list((root / "esphome" / "components").glob(prefix))
        assert comps, f"{pattern!r} matches no component"
        for comp in comps:
            test = root / "tests" / "components" / comp.name / "test.esp8266-ard.yaml"
            assert not test.exists(), (
                f"{comp.name} builds for ESP8266, drop {pattern!r}"
            )


def test_unbalanced_calls_are_reported_by_a_check_that_sees_every_file() -> None:
    # lint_log_no_bare_literal_ternary skips unbalanced calls and relies on this
    checks = {c["func"].__name__: c for c in ci_custom.LINT_CONTENT_CHECKS}
    continuation = checks["lint_log_multiline_continuation"]
    ternary = checks["lint_log_no_bare_literal_ternary"]
    assert continuation["exclude"] == []
    assert continuation["include"] == ternary["include"]


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
        ('ESP_LOGD(TAG, "%s", x ? /* c */ "on" : "off");', [(1, 33), (1, 40)]),
        (
            'ESP_LOGD(TAG, "%s",\n         x ? "on"  // NOLINT(some-clang-check)\n           : "off");',
            [(2, 14), (3, 14)],
        ),
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
