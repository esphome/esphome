"""Tests for elm327 component validation."""

from unittest.mock import patch

from esphome import config, yaml_util
from esphome.core import CORE

BASE_YAML = """
esphome:
  name: test

esp8266:
  board: esp01_1m

uart:
  tx_pin: GPIO1
  rx_pin: GPIO3
  baud_rate: 38400

sensor:
  - platform: elm327
    {sensors}
    update_interval: 10s
"""


def _load_and_validate(tmp_path, yaml_text):
    test_file = tmp_path / "test.yaml"
    test_file.write_text(yaml_text)
    parsed_yaml = yaml_util.load_yaml(test_file)
    with (
        patch.object(yaml_util, "load_yaml", return_value=parsed_yaml),
        patch.object(CORE, "config_path", test_file),
    ):
        return config.read_config({})


def test_elm327_all_sensors(tmp_path):
    """Test that all sensors together validate successfully."""
    sensors = """engine_rpm:
      name: Engine RPM
    vehicle_speed:
      name: Vehicle Speed
    coolant_temperature:
      name: Coolant Temperature
    engine_load:
      name: Engine Load
    throttle_position:
      name: Throttle Position
    intake_air_temperature:
      name: Intake Air Temperature
    maf_rate:
      name: MAF Rate
    fuel_level:
      name: Fuel Level
    battery_voltage:
      name: Battery Voltage"""
    yaml_text = BASE_YAML.format(sensors=sensors)
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, "Expected validation to succeed for all sensors"


def test_elm327_no_sensors_fails(tmp_path):
    """Test that configuring no sensors fails validation."""
    yaml_text = BASE_YAML.format(sensors="")
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is None, "Expected validation to fail when no sensors configured"


def test_elm327_custom_pid_hex_string(tmp_path):
    """Test that custom_pid with hex string PID validates successfully."""
    sensors = "custom_pid:\n      name: Custom\n      pid: '0xFF'"
    yaml_text = BASE_YAML.format(sensors=sensors)
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, (
        "Expected validation to succeed for custom_pid with hex string"
    )


def test_elm327_custom_pid_bare_hex(tmp_path):
    """Test that custom_pid with bare hex string (no 0x prefix) validates successfully."""
    sensors = "custom_pid:\n      name: Custom\n      pid: 'FF'"
    yaml_text = BASE_YAML.format(sensors=sensors)
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, (
        "Expected validation to succeed for custom_pid with bare hex"
    )


def test_elm327_custom_pid_integer(tmp_path):
    """Test that custom_pid with integer PID validates successfully."""
    sensors = "custom_pid:\n      name: Custom\n      pid: 255"
    yaml_text = BASE_YAML.format(sensors=sensors)
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, (
        "Expected validation to succeed for custom_pid with integer"
    )


def test_elm327_custom_pid_response_bytes(tmp_path):
    """Test that custom_pid with response_bytes validates successfully."""
    sensors = (
        "custom_pid:\n      name: Custom\n      pid: '0x22'\n      response_bytes: 2"
    )
    yaml_text = BASE_YAML.format(sensors=sensors)
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, (
        "Expected validation to succeed for custom_pid with response_bytes"
    )


def test_elm327_custom_pid_list(tmp_path):
    """Test that multiple custom_pid entries validate successfully."""
    sensors = """custom_pid:
      - name: First
        pid: '0x01'
      - name: Second
        pid: '0x02'
        response_bytes: 2"""
    yaml_text = BASE_YAML.format(sensors=sensors)
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, (
        "Expected validation to succeed for multiple custom_pid entries"
    )


def test_elm327_custom_pid_only_no_standard_sensors(tmp_path):
    """Test that custom_pid alone (no standard sensors) validates successfully."""
    sensors = "custom_pid:\n      name: Custom\n      pid: '0xAB'"
    yaml_text = BASE_YAML.format(sensors=sensors)
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, (
        "Expected validation to succeed with only custom_pid configured"
    )


def test_elm327_custom_pid_mode22(tmp_path):
    """Test that custom_pid with mode 22 and 2-byte PID validates successfully."""
    sensors = "custom_pid:\n      name: SoC\n      pid: '0xB201'\n      mode: 0x22\n      response_bytes: 2"
    yaml_text = BASE_YAML.format(sensors=sensors)
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, (
        "Expected validation to succeed for mode 22 with 2-byte PID"
    )


def test_elm327_custom_pid_invalid_pid_fails(tmp_path):
    """Test that an invalid PID value fails validation."""
    sensors = "custom_pid:\n      name: Custom\n      pid: '0x1FFFF'"
    yaml_text = BASE_YAML.format(sensors=sensors)
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is None, "Expected validation to fail for PID > 0xFFFF"


def test_elm327_custom_pid_invalid_response_bytes_fails(tmp_path):
    """Test that response_bytes out of range fails validation."""
    sensors = (
        "custom_pid:\n      name: Custom\n      pid: '0xFF'\n      response_bytes: 5"
    )
    yaml_text = BASE_YAML.format(sensors=sensors)
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is None, "Expected validation to fail for response_bytes > 4"


def test_elm327_custom_pid_zero_response_bytes_fails(tmp_path):
    """Test that response_bytes: 0 fails validation."""
    sensors = (
        "custom_pid:\n      name: Custom\n      pid: '0xFF'\n      response_bytes: 0"
    )
    yaml_text = BASE_YAML.format(sensors=sensors)
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is None, "Expected validation to fail for response_bytes < 1"


def test_elm327_custom_pid_invalid_mode_low_fails(tmp_path):
    """Test that mode: 0 fails validation."""
    sensors = "custom_pid:\n      name: Custom\n      pid: '0xFF'\n      mode: 0"
    yaml_text = BASE_YAML.format(sensors=sensors)
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is None, "Expected validation to fail for mode < 0x01"


def test_elm327_custom_pid_invalid_mode_high_fails(tmp_path):
    """Test that mode: 64 (0x40) fails validation."""
    sensors = "custom_pid:\n      name: Custom\n      pid: '0xFF'\n      mode: 64"
    yaml_text = BASE_YAML.format(sensors=sensors)
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is None, "Expected validation to fail for mode > 0x3F"


def test_elm327_custom_pid_non_hex_string_fails(tmp_path):
    """Test that a non-hex PID string fails validation."""
    sensors = "custom_pid:\n      name: Custom\n      pid: 'xyz'"
    yaml_text = BASE_YAML.format(sensors=sensors)
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is None, "Expected validation to fail for non-hex PID string"


def test_elm327_standard_and_custom_pid_together(tmp_path):
    """Test that standard sensors and custom_pid can be combined."""
    sensors = """engine_rpm:
      name: Engine RPM
    custom_pid:
      - name: SoC
        pid: '0xB201'
        mode: 0x22
        response_bytes: 2"""
    yaml_text = BASE_YAML.format(sensors=sensors)
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, (
        "Expected validation to succeed for standard + custom_pid"
    )
