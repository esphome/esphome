"""Unit tests for script/merge_component_configs.py deduplication."""

from pathlib import Path
import sys

import pytest

# Add the script directory to Python path so we can import the module
sys.path.insert(0, str((Path(__file__).parent / ".." / ".." / "script").resolve()))

import merge_component_configs  # noqa: E402

deduplicate_by_id = merge_component_configs.deduplicate_by_id


def test_identical_duplicate_ids_collapse() -> None:
    """Two identical items sharing an id collapse to one without error."""
    data = {
        "sensor": [
            {"id": "shared", "platform": "template", "name": "A"},
            {"id": "shared", "platform": "template", "name": "A"},
        ]
    }
    result = deduplicate_by_id(data)
    assert result["sensor"] == [{"id": "shared", "platform": "template", "name": "A"}]


def test_conflicting_duplicate_ids_raise() -> None:
    """Two different items sharing an id is a hard error naming the id."""
    data = {
        "sensor": [
            {"id": "dup", "platform": "template", "name": "A"},
            {"id": "dup", "platform": "template", "name": "B"},
        ]
    }
    with pytest.raises(ValueError, match="dup"):
        deduplicate_by_id(data)


def test_intentionally_shared_id_does_not_raise() -> None:
    """Allowlisted singleton ids may differ across components and collapse."""
    shared = next(iter(merge_component_configs.INTENTIONALLY_SHARED_IDS))
    data = {
        "time": [
            {"id": shared, "platform": "sntp"},
            {"id": shared, "platform": "sntp", "servers": ["a"]},
        ]
    }
    result = deduplicate_by_id(data)
    # First occurrence wins, no error raised
    assert result["time"] == [{"id": shared, "platform": "sntp"}]


def test_items_without_id_are_preserved() -> None:
    """Items lacking an id are passed through untouched."""
    data = {"binary_sensor": [{"platform": "gpio"}, {"platform": "gpio"}]}
    result = deduplicate_by_id(data)
    assert result["binary_sensor"] == [{"platform": "gpio"}, {"platform": "gpio"}]


def test_nested_lists_are_checked() -> None:
    """Conflicts nested inside dict values are also detected."""
    data = {
        "wrapper": {
            "sensor": [
                {"id": "dup", "value": 1},
                {"id": "dup", "value": 2},
            ]
        }
    }
    with pytest.raises(ValueError, match="dup"):
        deduplicate_by_id(data)
