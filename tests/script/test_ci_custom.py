"""Unit tests for the ESP_LOG-needs-braces lint rule in script/ci-custom.py.

The rule flags an if/else/for/while whose only body is an unbraced ESP_LOG*() call (which becomes an
empty statement -- and a -Wempty-body warning -- once the log level compiles the macro out). These
tests pin the comment/string/raw-string masker, the accepted control-statement shapes, and the
NOLINT escape hatch at both placements a contributor would try.
"""

import importlib.util
from pathlib import Path
import sys

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
