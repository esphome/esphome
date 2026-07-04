"""Unit tests for script/ci_check_duplicate_test_ids.py.

These lock in that the guard stays consistent with the actual config merge: it
prefixes substitutions the same way and delegates the conflict decision to
``merge_component_configs.deduplicate_by_id``.
"""

from pathlib import Path
import sys

import pytest

sys.path.insert(0, str((Path(__file__).parent / ".." / ".." / "script").resolve()))

import ci_check_duplicate_test_ids as checker  # noqa: E402


def _write_component(tests_dir: Path, name: str, body: str) -> None:
    comp = tests_dir / name
    comp.mkdir(parents=True)
    (comp / "test.esp32-idf.yaml").write_text(body)


@pytest.fixture
def tests_dir(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> Path:
    monkeypatch.setattr(checker, "TESTS_DIR", tmp_path)
    return tmp_path


def test_substitution_only_difference_is_a_conflict(tests_dir: Path) -> None:
    """Raw-identical items that differ only by a substitution still conflict.

    This is the class that the first version missed (and broke CI): the merge
    prefixes ``${pin}`` per component, so the two become ``${a_pin}`` and
    ``${b_pin}`` and collide.
    """
    shared = "sensor:\n  - platform: adc\n    id: shared\n    pin: ${pin}\n"
    _write_component(tests_dir, "comp_a", shared)
    _write_component(tests_dir, "comp_b", shared)
    result = checker.scan()
    assert any("shared" in line for line in result.conflicts), result.conflicts


def test_identical_substitution_free_items_do_not_conflict(tests_dir: Path) -> None:
    same = "sensor:\n  - platform: template\n    id: shared\n    name: Fixed\n"
    _write_component(tests_dir, "comp_a", same)
    _write_component(tests_dir, "comp_b", same)
    assert checker.scan().conflicts == []


def test_unique_ids_do_not_conflict(tests_dir: Path) -> None:
    _write_component(
        tests_dir,
        "comp_a",
        "sensor:\n  - platform: adc\n    id: comp_a_sensor\n    pin: ${pin}\n",
    )
    _write_component(
        tests_dir,
        "comp_b",
        "sensor:\n  - platform: adc\n    id: comp_b_sensor\n    pin: ${pin}\n",
    )
    assert checker.scan().conflicts == []


def test_same_list_key_under_different_paths_is_not_compared(tests_dir: Path) -> None:
    """Ids sharing a list key name but under different parent paths don't conflict.

    The merge only concatenates lists at the same path, so ``foo.shared`` and
    ``bar.shared`` are never compared against each other.
    """
    _write_component(
        tests_dir, "comp_a", "foo:\n  shared:\n    - id: dup\n      v: 1\n"
    )
    _write_component(
        tests_dir, "comp_b", "bar:\n  shared:\n    - id: dup\n      v: 2\n"
    )
    assert checker.scan().conflicts == []


def test_int_and_string_ids_are_distinct(tests_dir: Path) -> None:
    """``5`` and ``"5"`` are different ids, exactly as deduplicate_by_id treats them."""
    _write_component(tests_dir, "comp_a", "sensor:\n  - platform: t\n    id: 5\n")
    _write_component(tests_dir, "comp_b", 'sensor:\n  - platform: t\n    id: "5"\n')
    assert checker.scan().conflicts == []


def test_unparseable_fixture_is_reported_and_fails(tests_dir: Path) -> None:
    """A fixture that cannot be parsed is surfaced and fails the run, not skipped."""
    _write_component(tests_dir, "broken", "foo: [unbalanced\n")
    result = checker.scan()
    assert result.conflicts == []
    assert any("broken" in path for path in result.parse_errors)
    # The run as a whole must not pass when a covered fixture was not scanned.
    assert checker.main() == 1


def test_allowlisted_singleton_is_not_a_conflict(tests_dir: Path) -> None:
    """Ids in INTENTIONALLY_SHARED_IDS may differ across components."""
    _write_component(
        tests_dir, "comp_a", "time:\n  - platform: sntp\n    id: sntp_time\n"
    )
    _write_component(
        tests_dir,
        "comp_b",
        "time:\n  - platform: sntp\n    id: sntp_time\n    servers: [a.example]\n",
    )
    assert checker.scan().conflicts == []


def test_empty_scan_fails(tests_dir: Path) -> None:
    """A scan that covers zero fixtures is a false green and must fail."""
    result = checker.scan()
    assert result.components_scanned == 0
    assert checker.main() == 1
