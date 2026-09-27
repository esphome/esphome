"""Exercise real ESPHome final validation and code generation across platforms."""

from copy import deepcopy
from pathlib import Path
import subprocess
import sys

import pytest
import yaml

ROOT = Path(__file__).resolve().parents[3]


def base_config() -> dict:
    return {
        "esphome": {"name": "tmp102-test"},
        "esp32": {"board": "esp32dev"},
        "i2c": {"sda": "GPIO21", "scl": "GPIO22"},
        "sensor": [{"platform": "tmp102", "id": "chip", "name": "Temperature"}],
    }


def run_config(
    tmp_path: Path, config: dict, generate: bool = False
) -> subprocess.CompletedProcess:
    path = tmp_path / "test.yaml"
    path.write_text(yaml.safe_dump(config, sort_keys=False))
    command = [
        sys.executable,
        "-m",
        "esphome",
        "compile" if generate else "config",
        str(path),
    ]
    if generate:
        command.append("--only-generate")
    return subprocess.run(
        command, text=True, capture_output=True, check=False, cwd=ROOT
    )


def test_basic_unchanged(tmp_path: Path) -> None:
    config = base_config()
    result = run_config(tmp_path, config, generate=True)
    assert result.returncode == 0, result.stdout + result.stderr
    source = (tmp_path / ".esphome/build/tmp102-test/src/main.cpp").read_text()
    assert "set_configure" not in source
    assert "set_temperature_high" not in source
    defines = (
        tmp_path / ".esphome/build/tmp102-test/src/esphome/core/defines.h"
    ).read_text()
    assert "USE_TMP102_" not in defines


@pytest.mark.parametrize("kind", ["number", "binary_sensor", "text_sensor", "all"])
def test_optional_platforms(tmp_path: Path, kind: str) -> None:
    config = base_config()
    platforms = ["number", "binary_sensor", "text_sensor"] if kind == "all" else [kind]
    for platform in platforms:
        entry = {"platform": "tmp102", "tmp102_id": "chip"}
        if platform == "number":
            entry.update(
                temperature_high={"name": "High", "initial_value": 30},
                temperature_low={"name": "Low", "initial_value": 28},
            )
        else:
            entry["name"] = platform
        config[platform] = [entry]
    result = run_config(tmp_path, config, generate=True)
    assert result.returncode == 0, result.stdout + result.stderr
    defines = (
        tmp_path / ".esphome/build/tmp102-test/src/esphome/core/defines.h"
    ).read_text()
    for platform in ("number", "binary_sensor", "text_sensor"):
        assert (f"USE_TMP102_{platform.upper()}" in defines) == (platform in platforms)


@pytest.mark.parametrize(
    "case",
    [
        "inverted",
        "out_of_range",
        "duplicate_high",
        "duplicate_alert",
        "duplicate_status",
        "conflicting_initial",
        "missing_parent",
        "wrong_parent",
    ],
)
def test_invalid_cross_platform_config(tmp_path: Path, case: str) -> None:
    config = base_config()
    entry = {
        "platform": "tmp102",
        "tmp102_id": "chip",
        "temperature_high": {"name": "High", "initial_value": 30},
        "temperature_low": {"name": "Low", "initial_value": 28},
    }
    config["number"] = [entry]
    if case == "inverted":
        entry["temperature_low"]["initial_value"] = 31
    elif case == "out_of_range":
        entry["temperature_high"]["initial_value"] = 128
    elif case == "duplicate_high":
        other = deepcopy(entry)
        del other["temperature_low"]
        other["temperature_high"]["name"] = "Other high"
        config["number"].append(other)
    elif case in ("duplicate_alert", "duplicate_status"):
        platform = "binary_sensor" if case == "duplicate_alert" else "text_sensor"
        config[platform] = [
            {"platform": "tmp102", "tmp102_id": "chip", "name": name}
            for name in ("One", "Two")
        ]
    elif case == "conflicting_initial":
        config["sensor"][0]["temperature_high"] = 40
    elif case == "missing_parent":
        entry["tmp102_id"] = "nonexistent"
    elif case == "wrong_parent":
        config["sensor"].append(
            {"platform": "template", "id": "other", "name": "Other"}
        )
        entry["tmp102_id"] = "other"
    result = run_config(tmp_path, config)
    assert result.returncode != 0, result.stdout


def test_distinct_parents_and_extended_range(tmp_path: Path) -> None:
    config = base_config()
    config["sensor"].append(
        {
            "platform": "tmp102",
            "id": "hot_chip",
            "name": "Hot",
            "address": 0x49,
            "extended_mode": True,
            "temperature_high": 150,
            "temperature_low": 140,
        }
    )
    config["number"] = [
        {
            "platform": "tmp102",
            "tmp102_id": "chip",
            "temperature_high": {"name": "High"},
        },
        {
            "platform": "tmp102",
            "tmp102_id": "hot_chip",
            "temperature_high": {"name": "Hot High"},
        },
    ]
    result = run_config(tmp_path, config, generate=True)
    assert result.returncode == 0, result.stdout + result.stderr
    source = (tmp_path / ".esphome/build/tmp102-test/src/main.cpp").read_text()
    assert "127.9375" in source and "150.0" in source


def test_unrelated_entities_do_not_enable_tmp102_features(tmp_path: Path) -> None:
    """Global platform flags must not enable unused TMP102 entity support."""
    config = base_config()
    config["number"] = [
        {
            "platform": "template",
            "name": "Other Number",
            "min_value": 0,
            "max_value": 10,
            "step": 1,
            "optimistic": True,
        }
    ]
    config["binary_sensor"] = [{"platform": "template", "name": "Other Binary"}]
    config["text_sensor"] = [{"platform": "template", "name": "Other Text"}]
    result = run_config(tmp_path, config, generate=True)
    assert result.returncode == 0, result.stdout + result.stderr
    defines = (
        tmp_path / ".esphome/build/tmp102-test/src/esphome/core/defines.h"
    ).read_text()
    assert "USE_NUMBER" in defines
    assert "USE_BINARY_SENSOR" in defines
    assert "USE_TEXT_SENSOR" in defines
    assert "USE_TMP102_" not in defines


@pytest.mark.parametrize("number_first", [False, True])
def test_mixed_scalar_and_number_limits(tmp_path: Path, number_first: bool) -> None:
    """Cross-platform initial limits must not depend on YAML key order."""
    config = base_config()
    config["sensor"][0]["temperature_low"] = "9°F"
    config["number"] = [
        {
            "platform": "tmp102",
            "tmp102_id": "chip",
            "temperature_high": {"name": "High", "initial_value": "10°F"},
        }
    ]
    if number_first:
        config = {"number": config.pop("number"), **config}
    result = run_config(tmp_path, config, generate=True)
    assert result.returncode == 0, result.stdout + result.stderr
