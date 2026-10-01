"""Unit tests for script/split_components_for_ci.py module."""

from pathlib import Path
import sys
from unittest.mock import patch

import pytest

script_dir = str((Path(__file__).parent / ".." / ".." / "script").resolve())
sys.path.insert(0, script_dir)

import split_components_for_ci  # noqa: E402

import script.helpers  # noqa: E402


@pytest.fixture
def tests_dir(tmp_path: Path) -> Path:
    """Point the helpers at an empty tests/components tree."""
    path = tmp_path / "tests" / "components"
    path.mkdir(parents=True)
    with patch.object(script.helpers, "root_path", str(tmp_path)):
        yield path


def _add_component(tests_dir: Path, name: str, files: list[str]) -> None:
    comp_dir = tests_dir / name
    comp_dir.mkdir()
    for file in files:
        (comp_dir / file).write_text(f"# {name}\n")


def test_component_weight_counts_builds(tests_dir: Path) -> None:
    """Isolated components weigh per test file; groupable ones per variant."""
    _add_component(
        tests_dir,
        "multi",
        [
            "test.esp32-idf.yaml",
            "test.esp8266-ard.yaml",
            "test-extra.esp32-idf.yaml",
            "validate.esp32-idf.yaml",
        ],
    )
    _add_component(tests_dir, "single", ["test.esp32-idf.yaml"])

    build = split_components_for_ci.BUILD_WEIGHT
    group = split_components_for_ci.GROUPABLE_WEIGHT
    assert split_components_for_ci.component_weight("multi", True) == 3 * build
    assert split_components_for_ci.component_weight("multi", False) == group + build
    assert split_components_for_ci.component_weight("single", True) == build
    assert split_components_for_ci.component_weight("single", False) == group


def test_isolated_component_with_many_tests_gets_own_batch(tests_dir: Path) -> None:
    """A directly changed component with many test files is not stacked."""
    platforms = ["esp32-idf", "esp8266-ard", "rp2040-ard", "host", "bk72xx-ard"]
    for name in ("big_a", "big_b"):
        _add_component(
            tests_dir,
            name,
            [f"test.{p}.yaml" for p in platforms]
            + [f"test-variant.{p}.yaml" for p in platforms],
        )

    batches, _ = split_components_for_ci.create_intelligent_batches(
        components=["big_a", "big_b"],
        tests_dir=tests_dir,
        batch_size=40,
        directly_changed={"big_a", "big_b"},
    )

    assert sorted(batches) == [["big_a"], ["big_b"]]


def test_groupable_variants_split_batches(tests_dir: Path) -> None:
    """Variant files of groupable components count toward the batch size."""
    names = [f"comp_{i:02d}" for i in range(10)]
    for name in names:
        _add_component(
            tests_dir,
            name,
            ["test.esp32-idf.yaml", "test-a.esp32-idf.yaml", "test-b.esp32-idf.yaml"],
        )

    batches, _ = split_components_for_ci.create_intelligent_batches(
        components=names, tests_dir=tests_dir, batch_size=40
    )

    # Each component weighs 1 + 2 * 3 = 7, so at most 5 fit in a batch of 40
    assert len(batches) == 2
    assert sorted(c for batch in batches for c in batch) == names
