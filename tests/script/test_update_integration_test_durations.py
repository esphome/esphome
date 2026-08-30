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


def test_main_merges_partial_run(tmp_path: Path) -> None:
    """A partial run merges over the previous data instead of truncating it."""
    import json
    from unittest.mock import patch

    tests_dir = tmp_path / "tests" / "integration"
    tests_dir.mkdir(parents=True)
    for name in ("test_a", "test_b", "test_c"):
        (tests_dir / f"{name}.py").write_text("", encoding="utf-8")
    durations_file = tests_dir / "durations.json"
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
        patch.object(uitd, "root_path", str(tmp_path)),
        patch.object(uitd, "DURATIONS_FILE", durations_file),
    ):
        # 1 of 3 files covered: refused without --allow-partial
        with (
            patch.object(sys, "argv", ["uitd", str(junit_dir)]),
            pytest.raises(SystemExit),
        ):
            uitd.main()
        with patch.object(sys, "argv", ["uitd", str(junit_dir), "--allow-partial"]):
            assert uitd.main() == 0
    # test_a updated, test_b kept, deleted test_gone dropped
    assert json.loads(durations_file.read_text()) == {
        "tests/integration/test_a.py": 6.0,
        "tests/integration/test_b.py": 7.0,
    }
