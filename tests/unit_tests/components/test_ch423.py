"""Tests for ch423 component validation."""

from unittest.mock import patch

from esphome import config, yaml_util
from esphome.core import CORE


def test_ch423_mixed_gpio_modes_fails(tmp_path, capsys):
    """Test that mixing input/output on GPIO pins 0-7 fails validation."""
    test_file = tmp_path / "test.yaml"
    test_file.write_text("""
esphome:
  name: test

esp8266:
  board: esp01_1m

i2c:
  sda: GPIO4
  scl: GPIO5

ch423:
  - id: ch423_hub

binary_sensor:
  - platform: gpio
    name: "CH423 Input 0"
    pin:
      ch423: ch423_hub
      number: 0
      mode: input

switch:
  - platform: gpio
    name: "CH423 Output 1"
    pin:
      ch423: ch423_hub
      number: 1
      mode: output
""")

    parsed_yaml = yaml_util.load_yaml(test_file)

    with (
        patch.object(yaml_util, "load_yaml", return_value=parsed_yaml),
        patch.object(CORE, "config_path", test_file),
    ):
        result = config.read_config({})

    assert result is None, "Expected validation to fail with mixed GPIO modes"

    # Check that the error message mentions the GPIO pin restriction
    captured = capsys.readouterr()
    assert (
        "GPIO pins (0-7) must all be configured as input or all as output"
        in captured.out
    )
