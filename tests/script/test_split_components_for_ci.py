"""Unit tests for script/split_components_for_ci.py module."""

from collections.abc import Generator
from pathlib import Path
import sys
from unittest.mock import patch

import pytest

script_dir = str((Path(__file__).parent / ".." / ".." / "script").resolve())
sys.path.insert(0, script_dir)

import split_components_for_ci  # noqa: E402

import script.helpers  # noqa: E402


@pytest.fixture
def tests_dir(tmp_path: Path) -> Generator[Path, None, None]:
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


def test_build_seconds() -> None:
    """Known platforms use the table; nrf52 boards share one estimate."""
    build_seconds = split_components_for_ci.build_seconds
    assert build_seconds("host") == 10
    assert build_seconds("esp32-idf") == 45
    assert (
        build_seconds("nrf52-xiao-ble") == split_components_for_ci.NRF52_BUILD_SECONDS
    )
    assert (
        build_seconds("esp32-s3-idf") == split_components_for_ci.DEFAULT_BUILD_SECONDS
    )


def test_make_item_isolated_and_grouped(tests_dir: Path) -> None:
    """Isolated components own every build; groupable ones join shared builds."""
    _add_component(
        tests_dir,
        "comp",
        [
            "test.esp32-idf.yaml",
            "test.host.yaml",
            "test-extra.esp32-idf.yaml",
            "test-only.rp2040-ard.yaml",
            "validate.esp32-idf.yaml",
        ],
    )

    isolated = split_components_for_ci._make_item(
        tests_dir, "comp", "isolated_comp", True
    )
    assert isolated.own_seconds == 45 + 10 + 45 + 50
    assert isolated.grouped_builds == {}

    # The rp2040 variant has no base test to group with, so it always runs
    grouped = split_components_for_ci._make_item(tests_dir, "comp", "i2c", False)
    assert grouped.own_seconds == 50
    assert grouped.grouped_builds == {("i2c", "esp32-idf"): 90, ("i2c", "host"): 10}


def test_grouped_component_joins_existing_build() -> None:
    """A second member turns a lone build into a group that skips variants."""
    item = split_components_for_ci._BatchItem
    batch = split_components_for_ci._Batch()
    batch.add(item("a", 0, {("i2c", "esp32-idf"): 90}))
    assert batch.seconds == 90

    other = item("b", 0, {("i2c", "esp32-idf"): 45, ("i2c", "host"): 10})
    grouped = 45 + split_components_for_ci.GROUPED_COMPONENT_SECONDS
    assert batch.added_seconds(other) == grouped - 90 + 10
    batch.add(other)
    assert batch.seconds == grouped + 10


def test_isolated_components_balance_across_runners(tests_dir: Path) -> None:
    """Heavy isolated components spread out and light ones fill the gaps."""
    platforms = ["esp32-idf", "esp8266-ard", "rp2040-ard", "bk72xx-ard"]
    for name in ("big_a", "big_b"):
        _add_component(
            tests_dir,
            name,
            [f"test.{p}.yaml" for p in platforms]
            + [f"test-variant.{p}.yaml" for p in platforms],
        )
    for name in ("small_a", "small_b"):
        _add_component(tests_dir, name, ["test.esp32-idf.yaml"])

    batches, _ = split_components_for_ci.create_intelligent_batches(
        components=["big_a", "big_b", "small_a", "small_b"],
        tests_dir=tests_dir,
        target_seconds=500,
        directly_changed={"big_a", "big_b", "small_a", "small_b"},
    )

    assert sorted(sorted(batch) for batch in batches) == [
        ["big_a", "small_a"],
        ["big_b", "small_b"],
    ]


def test_groupable_variants_skipped_when_grouped(tests_dir: Path) -> None:
    """Grouped members skip their variants, so the group fits one runner."""
    names = [f"comp_{i:02d}" for i in range(12)]
    for name in names:
        _add_component(
            tests_dir,
            name,
            ["test.esp32-idf.yaml", "test-a.esp32-idf.yaml", "test-b.esp32-idf.yaml"],
        )

    batches, _ = split_components_for_ci.create_intelligent_batches(
        components=names, tests_dir=tests_dir, target_seconds=400
    )

    assert batches == [names]


def test_groupable_components_split_evenly(tests_dir: Path) -> None:
    """A group too large for one runner spreads evenly."""
    platforms = ["esp32-idf", "esp8266-ard", "rp2040-ard", "bk72xx-ard"]
    names = [f"comp_{i:02d}" for i in range(12)]
    for name in names:
        _add_component(tests_dir, name, [f"test.{p}.yaml" for p in platforms])

    batches, _ = split_components_for_ci.create_intelligent_batches(
        components=names, tests_dir=tests_dir, target_seconds=100
    )

    assert sorted(len(batch) for batch in batches) == [2, 2, 2, 3, 3]
    assert sorted(c for batch in batches for c in batch) == names


def test_make_item_reads_tests_dir(tmp_path: Path) -> None:
    """Costs come from the given tests_dir, not the repository tree."""
    other = tmp_path / "other"
    other.mkdir()
    _add_component(other, "comp", ["test.esp32-idf.yaml"])

    item = split_components_for_ci._make_item(other, "comp", "isolated_comp", True)
    assert item.own_seconds == 45


def test_balance_batches_empty() -> None:
    """No components means no runners."""
    assert split_components_for_ci.balance_batches([], 600) == []
