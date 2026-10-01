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
    """Isolated components own every build; groupable ones only variants."""
    _add_component(
        tests_dir,
        "comp",
        [
            "test.esp32-idf.yaml",
            "test.host.yaml",
            "test-extra.esp32-idf.yaml",
            "validate.esp32-idf.yaml",
        ],
    )

    isolated = split_components_for_ci._make_item("comp", "isolated_comp", True)
    assert isolated.own_seconds == 45 + 10 + 45
    assert isolated.grouped_builds == frozenset()

    grouped = split_components_for_ci._make_item("comp", "i2c", False)
    assert grouped.own_seconds == 45
    assert grouped.grouped_builds == {("i2c", "esp32-idf"), ("i2c", "host")}


def test_grouped_component_joins_existing_build() -> None:
    """Joining a grouped build a batch already has costs only the extra component."""
    item = split_components_for_ci._BatchItem
    batch = split_components_for_ci._Batch()
    batch.add(item("a", 0, frozenset({("i2c", "esp32-idf")})))
    other = item("b", 0, frozenset({("i2c", "esp32-idf"), ("i2c", "host")}))
    assert batch.added_seconds(other) == (
        split_components_for_ci.GROUPED_COMPONENT_SECONDS + 10
    )


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


def test_groupable_components_split_evenly(tests_dir: Path) -> None:
    """Groupable components with variants spread evenly over the runners."""
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

    assert [len(batch) for batch in batches] == [4, 4, 4]
    assert sorted(c for batch in batches for c in batch) == names


def test_balance_batches_empty() -> None:
    """No components means no runners."""
    assert split_components_for_ci.balance_batches([], 600) == []
