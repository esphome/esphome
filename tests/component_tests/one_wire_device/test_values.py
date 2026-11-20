"""Exercise model/channel validation through the real ESPHome configuration CLI."""

from pathlib import Path
import subprocess
import sys

import pytest

BASE = """esphome:
  name: value-contract
esp32:
  board: esp32dev
  framework:
    type: esp-idf
logger:
"""


@pytest.mark.parametrize(
    ("fragment", "message"),
    [
        (
            "one_wire_device:\n  pin: GPIO4\n  type: DS2401\n  value: 2\n",
            "no value channels",
        ),
        ("one_wire_device:\n  pin: GPIO4\n  values:\n    humidity: 40\n", "humidity"),
        (
            "one_wire_device:\n  pin: GPIO4\n  type: DS2413\n  values:\n    pio_a: 0.5\n",
            "0 or 1",
        ),
        (
            "one_wire_device:\n  id: emulated\n  pin: GPIO4\n  type: DS2413\ninterval:\n  - interval: 1s\n    then:\n      - one_wire_device.set_value:\n          id: emulated\n          value: 1\n",
            "channel is required",
        ),
        (
            "one_wire_device:\n  id: emulated\n  pin: GPIO4\ninterval:\n  - interval: 1s\n    then:\n      - one_wire_device.set_value:\n          id: emulated\n          channel: pio_a\n          value: 1\n",
            "does not support",
        ),
    ],
)
def test_model_rejects_inapplicable_values(tmp_path, fragment, message):
    config = tmp_path / "invalid.yaml"
    config.write_text(BASE + fragment)
    result = subprocess.run(
        [sys.executable, "-m", "esphome", "config", str(config)],
        cwd=Path(__file__).parents[3],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode != 0
    assert message in result.stdout + result.stderr
