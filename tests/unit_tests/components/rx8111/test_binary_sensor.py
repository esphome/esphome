"""Validate optional RX8111 status entities and parent association."""

from pathlib import Path

import pytest

from esphome.components.rx8111.binary_sensor import STATUS_SENSORS
from tests.unit_tests.components.rx8111.test_rx8111 import (
    configuration,
    validate_rx8111,
)


@pytest.mark.parametrize("key", STATUS_SENSORS)
def test_individual_status_sensor(tmp_path: Path, key: str) -> None:
    config = configuration(tmp_path)
    config["binary_sensor"] = [{"platform": "rx8111", key: {"name": key}}]
    result = validate_rx8111(tmp_path, config)
    assert not result.errors
    assert result["binary_sensor"][0][key]["entity_category"] == "diagnostic"
    if (device_class := STATUS_SENSORS[key]) is not None:
        assert result["binary_sensor"][0][key]["device_class"] == device_class


@pytest.mark.parametrize("binary", [False, True])
@pytest.mark.parametrize("text", [False, True])
def test_entity_combinations(tmp_path: Path, binary: bool, text: bool) -> None:
    config = configuration(tmp_path)
    config["time"][0]["update_interval"] = "never"
    if binary:
        config["binary_sensor"] = [
            {
                "platform": "rx8111",
                **{key: {"name": key} for key in STATUS_SENSORS},
            }
        ]
    if text:
        config["text_sensor"] = [{"platform": "rx8111", "name": "Timestamp"}]
    assert not validate_rx8111(tmp_path, config).errors


def test_empty_platform_rejected(tmp_path: Path) -> None:
    config = configuration(tmp_path)
    config["binary_sensor"] = [{"platform": "rx8111"}]
    assert validate_rx8111(tmp_path, config).errors


def test_duplicate_platform_rejected(tmp_path: Path) -> None:
    config = configuration(tmp_path)
    config["binary_sensor"] = [
        {"platform": "rx8111", "rx8111_id": "rtc", key: {"name": key}}
        for key in ("vlf", "evin")
    ]
    assert validate_rx8111(tmp_path, config).errors


def test_multiple_parents(tmp_path: Path) -> None:
    config = configuration(tmp_path)
    config["time"].append({"platform": "rx8111", "id": "other", "address": 0x33})
    config["binary_sensor"] = [
        {"platform": "rx8111", "rx8111_id": parent, "vlf": {"name": parent}}
        for parent in ("rtc", "other")
    ]
    assert not validate_rx8111(tmp_path, config).errors


def test_missing_parent_rejected(tmp_path: Path) -> None:
    config = configuration(tmp_path)
    config["binary_sensor"] = [
        {"platform": "rx8111", "rx8111_id": "missing", "vlf": {"name": "VLF"}}
    ]
    assert validate_rx8111(tmp_path, config).errors
