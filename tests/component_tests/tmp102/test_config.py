"""Exercise TMP102 sensor validation and code generation."""

from collections.abc import Callable
from pathlib import Path

import yaml

from esphome.core import CORE


def base_config() -> dict:
    return {
        "esphome": {"name": "tmp102-test"},
        "esp32": {"board": "esp32dev"},
        "i2c": {"sda": "GPIO21", "scl": "GPIO22"},
        "sensor": [{"platform": "tmp102", "id": "chip", "name": "Temperature"}],
    }


def write_config(tmp_path: Path, config: dict) -> Path:
    path = tmp_path / "test.yaml"
    path.write_text(yaml.safe_dump(config, sort_keys=False))
    return path


def test_basic_unchanged(tmp_path: Path, generate_main: Callable[[Path], str]) -> None:
    config = base_config()
    source = generate_main(write_config(tmp_path, config))
    assert "set_configure" not in source
    assert "set_temperature_high" not in source
    defines = " ".join(define.name for define in CORE.defines)
    assert "USE_TMP102_" not in defines


def test_advanced_options_enable_configuration_once(
    tmp_path: Path, generate_main: Callable[[Path], str]
) -> None:
    config = base_config()
    config["sensor"][0].update(
        extended_mode=False,
        conversion_rate="1Hz",
        one_shot_mode=False,
        alert_polarity="active_high",
        thermostat_mode="interrupt",
        fault_queue=2,
        temperature_high=30,
        temperature_low=25,
    )
    source = generate_main(write_config(tmp_path, config))
    assert source.count("set_configure(true)") == 1
