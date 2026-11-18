"""Tests for the lint in script/ci-custom.py that requires CONF_ and UNIT_ constants to be plain strings."""

import importlib.util
from pathlib import Path
import sys
from types import ModuleType

import pytest

SCRIPT_DIR = (Path(__file__).parent / ".." / ".." / "script").resolve()
sys.path.insert(0, str(SCRIPT_DIR))


def _load_ci_custom() -> ModuleType:
    spec = importlib.util.spec_from_file_location(
        "ci_custom", SCRIPT_DIR / "ci-custom.py"
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


ci_custom = _load_ci_custom()


def _lint(content: str) -> list[tuple[int, int, str]]:
    return ci_custom.lint_constant_is_plain_string(
        Path("esphome/components/x/sensor.py"), content
    )


@pytest.mark.parametrize(
    "content",
    [
        'CONF_FOO = "foo"\n',
        "UNIT_FOO = 'foo'\n",
        'CONF_FOO = "fo" "o"\n',
        'CONF_FOO: str = "foo"\n',
        "CONF_FOO: str\n",
        "UNIT_OF_MEASUREMENT_MAX_LENGTH = 63\n",
        "OTHER = 1 + 2\n",
        "ICON_FOO = some_call()\n",
        "MAX_FOO = CONF_BAR\n",
        "import os\n",
        'def f() -> None:\n    CONF_FOO = "a" + "b"\n',
        'class A:\n    CONF_FOO = f"{X}"\n',
        "a.CONF_FOO = 1\n",
        'try:\n    CONF_FOO = "a"\nexcept ImportError:\n    CONF_FOO = "b"\n',
        'if X:\n    def f() -> None:\n        CONF_FOO = "a" + "b"\n',
        "if X:\n    async def f() -> None:\n        CONF_FOO = X\n",
    ],
)
def test_accepts(content: str) -> None:
    assert _lint(content) == []


@pytest.mark.parametrize(
    ("content", "name", "col"),
    [
        ('UNIT_FOO = "a" + SUFFIX + "b"\n', "UNIT_FOO", 1),
        ('UNIT_FOO = "x" if A else "y"\n', "UNIT_FOO", 1),
        ('CONF_FOO = "a" + "b"\n', "CONF_FOO", 1),
        ('UNIT_FOO = "%s" % "b"\n', "UNIT_FOO", 1),
        ('UNIT_FOO = f"{X}"\n', "UNIT_FOO", 1),
        ('UNIT_FOO = b"x"\n', "UNIT_FOO", 1),
        ("CONF_FOO = OTHER_CONST\n", "CONF_FOO", 1),
        ("CONF_FOO = module.CONF_FOO\n", "CONF_FOO", 1),
        ("CONF_FOO = some_call()\n", "CONF_FOO", 1),
        ("CONF_FOO = 8\n", "CONF_FOO", 1),
        ('CONF_FOO = ["a", "b"]\n', "CONF_FOO", 1),
        ("CONF_FOO: str = OTHER\n", "CONF_FOO", 1),
        ('CONF_FOO += "x"\n', "CONF_FOO", 1),
        ('X = CONF_FOO = "a" + "b"\n', "CONF_FOO", 5),
        ('OTHER, CONF_FOO = "a", "b"\n', "CONF_FOO", 8),
        ('[OTHER, CONF_FOO] = "a", "b"\n', "CONF_FOO", 9),
        ('OTHER, *CONF_FOO = ["a", "b", "c"]\n', "CONF_FOO", 9),
        ('*CONF_FOO, = "xy"\n', "CONF_FOO", 2),
        ('OTHER, CONF_FOO = "ab"\n', "CONF_FOO", 8),
    ],
)
def test_rejects(content: str, name: str, col: int) -> None:
    errs = _lint(content)
    assert len(errs) == 1
    lineno, err_col, msg = errs[0]
    assert (lineno, err_col) == (1, col)
    assert name in msg
    assert "plain string literal" in msg


def test_reports_each_constant_at_its_own_line() -> None:
    content = 'CONF_A = "a"\nCONF_B = X\nOTHER = 1\nCONF_C, UNIT_D = Y\n'
    assert [(line, col) for line, col, _ in _lint(content)] == [
        (2, 1),
        (4, 1),
        (4, 9),
    ]


def test_checks_assignments_inside_module_level_blocks() -> None:
    content = (
        "if X:\n"
        "    CONF_A = A\n"
        "else:\n"
        "    CONF_B = B\n"
        "try:\n"
        "    CONF_C = C\n"
        "except ImportError:\n"
        "    CONF_D = D\n"
        "finally:\n"
        "    CONF_E = E\n"
        "with X:\n"
        "    CONF_F = F\n"
        "for x in X:\n"
        "    CONF_G = G\n"
        "match X:\n"
        "    case 1:\n"
        "        CONF_H = H\n"
    )
    assert [(line, col) for line, col, _ in _lint(content)] == [
        (2, 5),
        (4, 5),
        (6, 5),
        (8, 5),
        (10, 5),
        (12, 5),
        (14, 5),
        (17, 9),
    ]


def test_syntax_error_is_left_to_python_linters() -> None:
    assert _lint("CONF_FOO = (\n") == []


def _registered_check() -> dict:
    return next(
        check
        for check in ci_custom.LINT_CONTENT_CHECKS
        if check["func"] is ci_custom.lint_constant_is_plain_string
    )


@pytest.mark.parametrize(
    "fname",
    [
        "esphome/const.py",
        "esphome/components/const/__init__.py",
        "esphome/components/x/sensor.py",
        "esphome/core/config.py",
    ],
)
def test_runs_on_esphome_python_files(fname: str) -> None:
    errs = ci_custom.run_check(_registered_check(), fname, Path(fname), "CONF_A = B\n")
    assert len(errs) == 1


@pytest.mark.parametrize(
    "fname",
    [
        "tests/component_tests/x/test_x.py",
        "script/ci-custom.py",
        "esphome/components/x/x.cpp",
    ],
)
def test_skips_files_outside_esphome(fname: str) -> None:
    assert (
        ci_custom.run_check(_registered_check(), fname, Path(fname), "CONF_A = B\n")
        is None
    )
