"""Tests for rs485_frame sensor: per-decode default update_interval:.

esphbot review finding: RS485FrameSensor published on every value change with no polling
gate. frames_received and last_keepalive_ms change on essentially every RX frame (10-100/s
on a live bus), flooding the API connection and the HA recorder with state writes the
existing change-filter does nothing to reduce (the value genuinely changes every time).
Fix: RS485FrameSensor becomes a PollingComponent, and each decode: gets a sensible default
update_interval: -- 60s for the high-rate counters, 1s for decodes that change rarely (CRC
failures, command drops, response outcomes) so a real occurrence still surfaces promptly.
An explicit user update_interval: must not be overridden.
"""

from __future__ import annotations

from pathlib import Path

from esphome import config as esphome_config, yaml_util
from esphome.core import CORE


def _sensor_by_name(result, name: str) -> dict:
    for conf in result["sensor"]:
        if conf["name"] == name:
            return conf
    raise AssertionError(f"no sensor named {name!r} in validated config")


def test_high_rate_decode_defaults_to_sixty_second_update_interval(
    fixture_path: Path,
) -> None:
    """frames_received changes on essentially every RX frame -- must default to a coarse
    poll interval, not publish-on-every-frame."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_sensor_update_interval_defaults.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert not result.errors, f"expected no validation errors, got: {result.errors}"
    conf = _sensor_by_name(result, "Frames Received")
    assert conf["update_interval"].total_milliseconds == 60_000


def test_event_driven_decode_defaults_to_one_second_update_interval(
    fixture_path: Path,
) -> None:
    """crc_failures changes only on rare bus errors -- must default to a tight poll
    interval so a real failure isn't buried behind a minute-long wait."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_sensor_update_interval_defaults.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert not result.errors, f"expected no validation errors, got: {result.errors}"
    conf = _sensor_by_name(result, "CRC Failures")
    assert conf["update_interval"].total_milliseconds == 1_000


def test_explicit_update_interval_is_not_overridden_by_the_decode_default(
    fixture_path: Path,
) -> None:
    """A user-supplied update_interval: must win over the per-decode default, the same
    way an explicit state_class:/unit_of_measurement: already does."""
    CORE.config_path = fixture_path / "dummy.yaml"
    raw_config = yaml_util.load_yaml(
        fixture_path / "rs485_frame_sensor_update_interval_defaults.yaml"
    )

    result = esphome_config.validate_config(raw_config, {})

    assert not result.errors, f"expected no validation errors, got: {result.errors}"
    conf = _sensor_by_name(result, "Frames Received Custom")
    assert conf["update_interval"].total_milliseconds == 5_000
