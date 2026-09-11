"""Tests for the motion platforms' update_interval warning (shake, free_fall, moving)."""

import logging
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome import config, yaml_util
from esphome.core import CORE

CONFIG_TEMPLATE = """
esphome:
  name: test

esp8266:
  board: esp01_1m

i2c:
  sda: GPIO4
  scl: GPIO5

motion:
  - platform: lsm6ds
    id: lsm6ds_motion
    update_interval: {update_interval}

{platform_config}
"""

EVENT_CONFIG = """
event:
  - platform: motion
    motion_id: lsm6ds_motion
    name: "Shake event"
"""

FREE_FALL_CONFIG = """
binary_sensor:
  - platform: motion
    motion_id: lsm6ds_motion
    type: free_fall
    name: "Free fall"
"""

MOVING_CONFIG = """
binary_sensor:
  - platform: motion
    motion_id: lsm6ds_motion
    type: moving
    name: "Moving"
"""

FACE_UP_CONFIG = """
binary_sensor:
  - platform: motion
    motion_id: lsm6ds_motion
    type: face_up
    name: "Face up"
"""


def _read_config(tmp_path: Path, update_interval: str, platform_config: str):
    test_file = tmp_path / "test.yaml"
    test_file.write_text(
        CONFIG_TEMPLATE.format(
            update_interval=update_interval, platform_config=platform_config
        )
    )

    parsed_yaml = yaml_util.load_yaml(test_file)

    with (
        patch.object(yaml_util, "load_yaml", return_value=parsed_yaml),
        patch.object(CORE, "config_path", test_file),
    ):
        return config.read_config({})


@pytest.mark.parametrize(
    ("platform_config", "feature_name"),
    [
        (EVENT_CONFIG, "shake"),
        (FREE_FALL_CONFIG, "free-fall"),
        (MOVING_CONFIG, "moving"),
    ],
    ids=["shake", "free_fall", "moving"],
)
def test_warns_on_slow_update_interval(
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
    platform_config: str,
    feature_name: str,
) -> None:
    """A parent update_interval slower than 100ms should log a warning."""
    with caplog.at_level(logging.WARNING):
        result = _read_config(tmp_path, "250ms", platform_config)

    assert result is not None, "Slow update_interval should still be a valid config"
    warning_text = (
        f"{feature_name} detection works best with an update_interval of 100ms or less"
    )
    assert any(warning_text in record.message for record in caplog.records)


@pytest.mark.parametrize(
    ("platform_config", "feature_name"),
    [
        (EVENT_CONFIG, "shake"),
        (FREE_FALL_CONFIG, "free-fall"),
        (MOVING_CONFIG, "moving"),
    ],
    ids=["shake", "free_fall", "moving"],
)
def test_no_warning_on_fast_update_interval(
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
    platform_config: str,
    feature_name: str,
) -> None:
    """A parent update_interval of 100ms or less should not log a warning."""
    with caplog.at_level(logging.WARNING):
        result = _read_config(tmp_path, "50ms", platform_config)

    assert result is not None
    warning_text = f"{feature_name} detection works best with an update_interval"
    assert not any(warning_text in record.message for record in caplog.records)


def test_no_warning_for_face_up_with_slow_update_interval(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """face_up/face_down track steady orientation, not a fast event, so no warning."""
    with caplog.at_level(logging.WARNING):
        result = _read_config(tmp_path, "250ms", FACE_UP_CONFIG)

    assert result is not None
    assert not any(
        "detection works best with an update_interval" in record.message
        for record in caplog.records
    )
