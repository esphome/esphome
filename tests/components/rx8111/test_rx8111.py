"""RX8111 time-only schema and native alarm regressions."""

import os
from pathlib import Path
import subprocess
import sys

import pytest

from esphome.config import validate_config
from esphome.core import CORE, Lambda
from esphome.yaml_util import dump

ROOT = Path(__file__).resolve().parent


def configuration(tmp_path: Path, actions: list | None = None) -> dict:
    return {
        "esphome": {"name": "alarm-test", "build_path": str(tmp_path / "build")},
        "host": {},
        "logger": {},
        "i2c": {"device": "/dev/i2c-0", "scan": False},
        "time": [{"platform": "rx8111", "id": "rtc", "alarm_enabled": True}],
        "script": [
            {
                "id": "alarm_actions",
                "then": actions or [{"rx8111.unset_alarm": {"id": "rtc"}}],
            }
        ],
    }


def validate_rx8111(tmp_path: Path, config: dict):
    CORE.reset()
    CORE.config_path = tmp_path / "test.yaml"
    return validate_config(config, {})


def test_unset(tmp_path: Path) -> None:
    assert not validate_rx8111(tmp_path, configuration(tmp_path)).errors


@pytest.mark.parametrize("enabled", [True, False, Lambda("return true;")])
def test_enabled_removed(tmp_path: Path, enabled: bool | Lambda) -> None:
    actions = [{"rx8111.set_alarm": {"id": "rtc", "hour": 12, "enabled": enabled}}]
    assert any(
        "'enabled' was removed" in str(error)
        for error in validate_rx8111(tmp_path, configuration(tmp_path, actions)).errors
    )


@pytest.mark.parametrize("hour", [12, Lambda("return 12;")])
def test_set_literals_and_templates(tmp_path: Path, hour: int | Lambda) -> None:
    actions = [{"rx8111.set_alarm": {"id": "rtc", "hours": hour, "minute": 0}}]
    assert not validate_rx8111(tmp_path, configuration(tmp_path, actions)).errors


@pytest.mark.parametrize(
    "fields", [{}, {"weekday": 0}, {"weekday": 1, "day": 5}, {"hour": 24}]
)
def test_invalid_set(tmp_path: Path, fields: dict) -> None:
    actions = [{"rx8111.set_alarm": {"id": "rtc", **fields}}]
    assert validate_rx8111(tmp_path, configuration(tmp_path, actions)).errors


def test_timer_callback_requires_count(tmp_path: Path) -> None:
    config = configuration(tmp_path)
    config["time"][0]["on_timer"] = [{"logger.log": "timer"}]
    assert validate_rx8111(tmp_path, config).errors
    config["time"][0]["timer_enabled"] = False
    assert not validate_rx8111(tmp_path, config).errors


@pytest.mark.parametrize(
    "key,value",
    [
        ("model", "RX8130CE"),
        ("digital_offset", 2),
        ("timer_clock", "1_3600hz"),
        ("timer_count", 16_777_216),
    ],
)
def test_reject_unsupported(tmp_path: Path, key: str, value: object) -> None:
    config = configuration(tmp_path)
    config["time"][0][key] = value
    assert validate_rx8111(tmp_path, config).errors


def compile_and_run(tmp_path: Path, config: dict, expected: str) -> None:
    source = tmp_path / "test.yaml"
    source.write_text(dump(config), encoding="utf-8")
    result = subprocess.run(
        [sys.executable, "-m", "esphome", "compile", str(source)],
        env={**os.environ, "PYTHONPATH": str(ROOT.parents[2])},
        capture_output=True,
        text=True,
        timeout=300,
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    executable = tmp_path / "build" / ".pioenvs" / "alarm-test" / "program"
    result = subprocess.run(
        [str(executable)], capture_output=True, text=True, timeout=20, check=False
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert expected in result.stdout


@pytest.mark.skipif(
    os.environ.get("RX8111_BUILD_TESTS") != "1",
    reason="Set RX8111_BUILD_TESTS=1 for native build",
)
def test_native_alarm_actions(tmp_path: Path) -> None:
    config = configuration(
        tmp_path,
        actions=[
            {
                "rx8111.set_alarm": {
                    "id": "rtc",
                    "hour": Lambda("return 12;"),
                    "minute": 0,
                }
            },
            {"rx8111.unset_alarm": {"id": "rtc"}},
            {"rx8111.clear_alarm_flag": {"id": "rtc"}},
            {"rx8111.schedule_alarm_in": {"id": "rtc", "seconds": 30}},
        ],
    )
    config["esphome"].update(
        {
            "includes": [str(ROOT / "alarm_actions.h")],
            "platformio_options": {
                "build_flags": ["-Wl,--wrap=time", "-Wl,--wrap=settimeofday"]
            },
            "on_boot": {"priority": 900, "then": [{"lambda": "alarm_test::run();"}]},
        }
    )
    compile_and_run(tmp_path, config, "Alarm action tests passed")
