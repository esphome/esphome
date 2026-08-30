"""Unit tests for script/update_integration_test_durations.py."""

from pathlib import Path
import sys

import pytest

# Add the script directory to Python path so we can import the module
script_dir = str((Path(__file__).parent / ".." / ".." / "script").resolve())
sys.path.insert(0, script_dir)

import update_integration_test_durations as uitd  # noqa: E402

JUNIT_TEMPLATE = """<?xml version="1.0" encoding="utf-8"?>
<testsuites><testsuite>{testcases}</testsuite></testsuites>
"""


def _write_junit(path: Path, testcases: str) -> None:
    path.write_text(JUNIT_TEMPLATE.format(testcases=testcases), encoding="utf-8")


def test_collect_durations_sums_per_file(tmp_path: Path) -> None:
    """Testcases from the same module sum; other suites are ignored."""
    existing = sorted(Path(uitd.root_path, "tests/integration").glob("test_*.py"))[:2]
    mod_a, mod_b = existing[0].stem, existing[1].stem
    _write_junit(
        tmp_path / "a.xml",
        f'<testcase classname="tests.integration.{mod_a}" name="t1" time="1.5"/>'
        f'<testcase classname="tests.integration.{mod_a}" name="t2" time="2.0"/>'
        f'<testcase classname="tests.integration.{mod_b}" name="t1" time="4.0"/>'
        f'<testcase classname="tests.unit_tests.test_x" name="t1" time="9.0"/>'
        f'<testcase classname="" name="anon" time="9.0"/>',
    )
    durations = uitd.collect_durations(tmp_path)
    assert durations == {
        f"tests/integration/{mod_a}.py": 3.5,
        f"tests/integration/{mod_b}.py": 4.0,
    }


def test_collect_durations_class_based_testcase(tmp_path: Path) -> None:
    """A class-based classname still maps to its module file."""
    module = next(Path(uitd.root_path, "tests/integration").glob("test_*.py")).stem
    _write_junit(
        tmp_path / "a.xml",
        f'<testcase classname="tests.integration.{module}.TestFoo" name="t" time="2.5"/>',
    )
    assert uitd.collect_durations(tmp_path) == {f"tests/integration/{module}.py": 2.5}


def test_collect_durations_unknown_module_skipped(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A classname that maps to no file on disk is skipped with a warning."""
    _write_junit(
        tmp_path / "a.xml",
        '<testcase classname="tests.integration.test_gone_forever" name="t" time="2.5"/>',
    )
    assert uitd.collect_durations(tmp_path) == {}
    assert "test_gone_forever" in capsys.readouterr().err


def test_collect_durations_empty_dir_aborts(tmp_path: Path) -> None:
    """No junit XML at all is a hard error, not an empty recording."""
    with pytest.raises(SystemExit):
        uitd.collect_durations(tmp_path)
