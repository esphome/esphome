"""Unit tests for script/update_integration_test_durations.py."""

import json
from pathlib import Path
import sys
from unittest.mock import patch

import pytest

# Add the script directory to Python path so we can import the module
script_dir = str((Path(__file__).parent / ".." / ".." / "script").resolve())
sys.path.insert(0, script_dir)

import helpers  # noqa: E402
import update_integration_test_durations as uitd  # noqa: E402

JUNIT_TEMPLATE = """<?xml version="1.0" encoding="utf-8"?>
<testsuites><testsuite>{testcases}</testsuite></testsuites>
"""

KNOWN = {
    "tests/integration/test_a.py",
    "tests/integration/test_b.py",
}


def _write_junit(path: Path, testcases: str) -> None:
    path.write_text(JUNIT_TEMPLATE.format(testcases=testcases), encoding="utf-8")


def test_collect_durations_sums_per_file(tmp_path: Path) -> None:
    """Testcases from the same module sum."""
    _write_junit(
        tmp_path / "a.xml",
        '<testcase classname="tests.integration.test_a" name="t1" time="1.5"/>'
        '<testcase classname="tests.integration.test_a" name="t2" time="2.0"/>'
        '<testcase classname="tests.integration.test_b" name="t1" time="4.0"/>',
    )
    assert uitd.collect_durations(tmp_path, KNOWN) == {
        "tests/integration/test_a.py": 3.5,
        "tests/integration/test_b.py": 4.0,
    }


def test_collect_durations_class_based_testcase(tmp_path: Path) -> None:
    """A class-based classname still maps to its module file."""
    _write_junit(
        tmp_path / "a.xml",
        '<testcase classname="tests.integration.test_a.TestFoo" name="t" time="2.5"/>',
    )
    assert uitd.collect_durations(tmp_path, KNOWN) == {
        "tests/integration/test_a.py": 2.5
    }


def test_collect_durations_unknown_module_skipped(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    """A classname that maps to no known file is skipped with a warning."""
    _write_junit(
        tmp_path / "a.xml",
        '<testcase classname="tests.integration.test_gone" name="t" time="2.5"/>',
    )
    assert uitd.collect_durations(tmp_path, KNOWN) == {}
    assert "test_gone" in capsys.readouterr().err


def test_collect_durations_skips_skipped_testcases(tmp_path: Path) -> None:
    """Skipped testcases do not record a bogus zero duration."""
    _write_junit(
        tmp_path / "a.xml",
        '<testcase classname="tests.integration.test_a" name="t" time="0">'
        "<skipped/></testcase>",
    )
    assert uitd.collect_durations(tmp_path, KNOWN) == {}


def test_collect_durations_unexpected_classname_aborts(tmp_path: Path) -> None:
    """A classname outside tests.integration means the junit layout changed."""
    _write_junit(
        tmp_path / "a.xml",
        '<testcase classname="tests.unit_tests.test_x" name="t" time="9.0"/>',
    )
    with pytest.raises(SystemExit):
        uitd.collect_durations(tmp_path, KNOWN)


def test_collect_durations_empty_dir_aborts(tmp_path: Path) -> None:
    """No junit XML at all is a hard error, not an empty recording."""
    with pytest.raises(SystemExit):
        uitd.collect_durations(tmp_path, KNOWN)


def test_main_merges_partial_run(tmp_path: Path) -> None:
    """A partial run merges over the previous data instead of truncating it."""
    tests_dir = tmp_path / "tests" / "integration"
    tests_dir.mkdir(parents=True)
    for name in ("test_a", "test_b", "test_c"):
        (tests_dir / f"{name}.py").write_text("", encoding="utf-8")
    durations_file = tmp_path / helpers.INTEGRATION_TEST_DURATIONS_FILE
    durations_file.write_text(
        json.dumps(
            {
                "tests/integration/test_a.py": 5.0,
                "tests/integration/test_b.py": 7.0,
                "tests/integration/test_gone.py": 9.0,
            }
        ),
        encoding="utf-8",
    )
    junit_dir = tmp_path / "junit"
    junit_dir.mkdir()
    _write_junit(
        junit_dir / "a.xml",
        '<testcase classname="tests.integration.test_a" name="t" time="6.0"/>',
    )
    with (
        patch.object(helpers, "root_path", str(tmp_path)),
        patch.object(uitd, "DURATIONS_FILE", durations_file),
    ):
        # 1 of 3 files covered: refused without --allow-partial
        with patch.object(sys, "argv", ["uitd", str(junit_dir)]):
            assert uitd.main() == uitd.EXIT_LOW_COVERAGE
        with patch.object(sys, "argv", ["uitd", str(junit_dir), "--allow-partial"]):
            assert uitd.main() == 0
    # test_a updated, test_b kept, deleted test_gone dropped
    assert json.loads(durations_file.read_text()) == {
        "tests/integration/test_a.py": 6.0,
        "tests/integration/test_b.py": 7.0,
    }
