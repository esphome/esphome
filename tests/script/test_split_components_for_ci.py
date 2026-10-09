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
    assert build_seconds("esp32-idf") == 55
    assert (
        build_seconds("nrf52-xiao-ble") == split_components_for_ci.NRF52_BUILD_SECONDS
    )
    assert (
        build_seconds("esp32-c5-idf") == split_components_for_ci.DEFAULT_BUILD_SECONDS
    )
    assert (
        build_seconds("esp32-c3-ard")
        == split_components_for_ci.ESP32_ARDUINO_BUILD_SECONDS
    )


def test_heavy_components(tests_dir: Path) -> None:
    """A heavy component, or a test that enables one, costs its library time."""
    heavy_components = split_components_for_ci.heavy_components
    heavy_seconds = split_components_for_ci.heavy_seconds
    table = split_components_for_ci.HEAVY_COMPONENT_SECONDS
    _add_component(tests_dir, "dht", ["test.esp32-idf.yaml"])
    _add_component(tests_dir, "micro_wake_word", ["test.esp32-idf.yaml"])
    assert heavy_components("dht", tests_dir / "dht/test.esp32-idf.yaml") == set()
    assert heavy_components(
        "micro_wake_word", tests_dir / "micro_wake_word/test.esp32-idf.yaml"
    ) == {"micro_wake_word"}
    assert heavy_seconds(frozenset({"micro_wake_word"})) == table["micro_wake_word"]

    display = tests_dir / "mipi_spi"
    display.mkdir()
    (display / "common.yaml").write_text("web_server:\n  port: 80\n")
    (display / "test-lvgl.esp32-s3-idf.yaml").write_text(
        "packages:\n"
        "  mipi_spi: !include common.yaml\n"
        "lvgl:\n  pages: []\n"
        "light:\n  - platform: fastled_spi\n    num_leds: 1\n"
    )
    assert heavy_components("mipi_spi", display / "test-lvgl.esp32-s3-idf.yaml") == {
        "lvgl",
        "web_server",
        "fastled_spi",
    }


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
    assert isolated.own_seconds == 55 + 10 + 55 + 45
    assert isolated.grouped_builds == {}

    # The rp2040 variant has no base test to group with, so it always runs
    grouped = split_components_for_ci._make_item(tests_dir, "comp", "i2c", False)
    assert grouped.own_seconds == 45
    assert grouped.grouped_builds == {
        ("i2c", "esp32-idf"): (110, frozenset()),
        ("i2c", "host"): (10, frozenset()),
    }


def test_make_item_heavy_component(tests_dir: Path) -> None:
    """A heavy component's library cost follows it into every build."""
    _add_component(tests_dir, "lvgl", ["test.esp32-idf.yaml", "test-a.host.yaml"])
    extra = split_components_for_ci.HEAVY_COMPONENT_SECONDS["lvgl"]

    item = split_components_for_ci._make_item(tests_dir, "lvgl", "spi", False)
    assert item.own_seconds == 10 + extra
    assert item.grouped_builds == {
        ("spi", "esp32-idf"): (55 + extra, frozenset({"lvgl"}))
    }


def test_heavy_cost_is_per_platform(tests_dir: Path) -> None:
    """A base test that enables a heavy component only charges its own platform."""
    comp = tests_dir / "web_server_idf"
    comp.mkdir()
    (comp / "test.esp32-idf.yaml").write_text("web_server:\n  port: 80\n")
    (comp / "test.host.yaml").write_text("logger:\n")
    extra = split_components_for_ci.HEAVY_COMPONENT_SECONDS["web_server"]

    item = split_components_for_ci._make_item(
        tests_dir, "web_server_idf", "none", False
    )
    assert item.grouped_builds == {
        ("none", "esp32-idf"): (55 + extra, frozenset({"web_server"})),
        ("none", "host"): (10, frozenset()),
    }


def test_grouped_component_joins_existing_build() -> None:
    """A second member turns a lone build into a group that skips variants."""
    item = split_components_for_ci._BatchItem
    share = split_components_for_ci._GroupedShare
    batch = split_components_for_ci._Batch()
    batch.add(item("a", 0, {("i2c", "esp32-idf"): share(110, frozenset())}))
    assert batch.seconds == 110

    other = item(
        "b",
        0,
        {
            ("i2c", "esp32-idf"): share(55, frozenset()),
            ("i2c", "host"): share(10, frozenset()),
        },
    )
    grouped = 55 + split_components_for_ci.GROUPED_COMPONENT_SECONDS
    assert batch.added_seconds(other) == grouped - 110 + 10
    batch.add(other)
    assert batch.seconds == grouped + 10


def test_heavy_component_charges_the_shared_build() -> None:
    """A heavy member makes the whole grouped build slower, once per library."""
    item = split_components_for_ci._BatchItem
    share = split_components_for_ci._GroupedShare
    batch = split_components_for_ci._Batch()
    extra = split_components_for_ci.HEAVY_COMPONENT_SECONDS["lvgl"]
    per_member = split_components_for_ci.GROUPED_COMPONENT_SECONDS
    lvgl = frozenset({"lvgl"})
    batch.add(item("a", 0, {("spi", "esp32-idf"): share(55, frozenset())}))

    heavy = item("lvgl", 0, {("spi", "esp32-idf"): share(55 + extra, lvgl)})
    grouped = 55 + per_member + extra
    assert batch.added_seconds(heavy) == grouped - 55
    batch.add(heavy)
    assert batch.seconds == grouped

    light = item("b", 0, {("spi", "esp32-idf"): share(55, frozenset())})
    assert batch.added_seconds(light) == per_member

    # A second member that also draws with lvgl compiles the library once
    also_heavy = item("c", 0, {("spi", "esp32-idf"): share(55 + extra, lvgl)})
    assert batch.added_seconds(also_heavy) == per_member


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

    assert sorted(len(batch) for batch in batches) == [3, 3, 3, 3]
    assert sorted(c for batch in batches for c in batch) == names


def test_make_item_reads_tests_dir(tmp_path: Path) -> None:
    """Costs come from the given tests_dir, not the repository tree."""
    other = tmp_path / "other"
    other.mkdir()
    _add_component(other, "comp", ["test.esp32-idf.yaml"])

    item = split_components_for_ci._make_item(other, "comp", "isolated_comp", True)
    assert item.own_seconds == 55


def test_balance_batches_keeps_partners_together() -> None:
    """Two components sharing a build land on one runner even when the
    lightest runner is the other one."""
    share = {
        ("i2c", "esp32-idf"): split_components_for_ci._GroupedShare(55, frozenset()),
        ("i2c", "esp8266-ard"): split_components_for_ci._GroupedShare(30, frozenset()),
    }
    items = [
        split_components_for_ci._BatchItem("partner_a", 0, dict(share)),
        split_components_for_ci._BatchItem("partner_b", 0, dict(share)),
        split_components_for_ci._BatchItem("alone", 80),
    ]

    batches = split_components_for_ci.balance_batches(items, target_seconds=120)

    assert sorted(batches) == [["alone"], ["partner_a", "partner_b"]]


def test_balance_batches_empty() -> None:
    """No components means no runners."""
    assert split_components_for_ci.balance_batches([], 600) == []
