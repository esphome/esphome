"""Basic RX8111 configuration regressions."""

from pathlib import Path

import pytest

from esphome.config import validate_config
from esphome.core import CORE


def configuration(tmp_path: Path, actions: list | None = None) -> dict:
    return {
        "esphome": {"name": "rx8111-clock-test", "build_path": str(tmp_path / "build")},
        "esp32": {"board": "esp32dev", "framework": {"type": "esp-idf"}},
        "logger": {},
        "i2c": {"sda": 21, "scl": 22, "scan": False},
        "time": [{"platform": "rx8111", "id": "rtc"}],
        "script": [
            {
                "id": "clock_actions",
                "then": actions or [{"rx8111.read_time": {"id": "rtc"}}],
            }
        ],
    }


def validate_rx8111(tmp_path: Path, config: dict):
    CORE.reset()
    CORE.config_path = tmp_path / "test.yaml"
    return validate_config(config, {})


@pytest.mark.parametrize("action", ["rx8111.read_time", "rx8111.write_time"])
def test_actions(tmp_path: Path, action: str) -> None:
    for payload in ({}, {"id": "rtc"}):
        assert not validate_rx8111(
            tmp_path, configuration(tmp_path, [{action: payload}])
        ).errors
