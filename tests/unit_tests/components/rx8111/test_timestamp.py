"""Validate the timestamp entity against real, portable RX8111 configurations."""

from pathlib import Path

import pytest

from tests.unit_tests.components.rx8111.test_rx8111 import (
    configuration,
    validate_rx8111,
)


@pytest.mark.parametrize("mode", ["latest", "stop_when_full", "overwrite"])
@pytest.mark.parametrize("enabled", [None, False, True])
def test_timestamp_modes(tmp_path: Path, mode: str, enabled: bool | None) -> None:
    config = configuration(tmp_path)
    config["time"][0]["event_level"] = "high"
    if enabled is not None:
        config["time"][0]["event_enabled"] = enabled
    config["text_sensor"] = [
        {"platform": "rx8111", "name": "Timestamp", "timestamp_record_mode": mode}
    ]
    assert not validate_rx8111(tmp_path, config).errors


def test_duplicate_timestamp_rejected(tmp_path: Path) -> None:
    config = configuration(tmp_path)
    config["text_sensor"] = [
        {"platform": "rx8111", "name": name, "rx8111_id": "rtc"}
        for name in ("First", "Second")
    ]
    assert validate_rx8111(tmp_path, config).errors


@pytest.mark.parametrize(
    ("option", "value"),
    [("event_level", "high"), ("evin_filter", 2), ("evin_pull", "none")],
)
def test_evin_settings_without_consumer_rejected(
    tmp_path: Path, option: str, value: str | int
) -> None:
    config = configuration(tmp_path)
    config["time"][0][option] = value
    assert validate_rx8111(tmp_path, config).errors


def test_invalid_timestamp_mode_rejected(tmp_path: Path) -> None:
    config = configuration(tmp_path)
    config["text_sensor"] = [
        {"platform": "rx8111", "name": "Timestamp", "timestamp_record_mode": "invalid"}
    ]
    assert validate_rx8111(tmp_path, config).errors


def test_timestamp_multiple_clocks(tmp_path: Path) -> None:
    config = configuration(tmp_path)
    config["time"].append({"platform": "rx8111", "id": "other", "address": 0x33})
    config["text_sensor"] = [
        {"platform": "rx8111", "name": clock, "rx8111_id": clock}
        for clock in ("rtc", "other")
    ]
    assert not validate_rx8111(tmp_path, config).errors


def test_timestamp_requires_rx8111_clock(tmp_path: Path) -> None:
    config = configuration(tmp_path)
    config.pop("time")
    config.pop("script")
    config["text_sensor"] = [{"platform": "rx8111", "name": "Timestamp"}]
    assert validate_rx8111(tmp_path, config).errors
