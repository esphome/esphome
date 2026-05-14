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


def test_elm327_engine_rpm_sensor(tmp_path):
    """Test that engine_rpm sensor validates successfully."""
    yaml_text = BASE_YAML.format(sensors="engine_rpm:\n      name: Engine RPM")
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, "Expected validation to succeed for engine_rpm sensor"


def test_elm327_vehicle_speed_sensor(tmp_path):
    """Test that vehicle_speed sensor validates successfully."""
    yaml_text = BASE_YAML.format(sensors="vehicle_speed:\n      name: Vehicle Speed")
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, "Expected validation to succeed for vehicle_speed sensor"


def test_elm327_coolant_temperature_sensor(tmp_path):
    """Test that coolant_temperature sensor validates successfully."""
    yaml_text = BASE_YAML.format(
        sensors="coolant_temperature:\n      name: Coolant Temp"
    )
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, "Expected validation to succeed for coolant_temperature"


def test_elm327_engine_load_sensor(tmp_path):
    """Test that engine_load sensor validates successfully."""
    yaml_text = BASE_YAML.format(sensors="engine_load:\n      name: Engine Load")
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, "Expected validation to succeed for engine_load"


def test_elm327_throttle_position_sensor(tmp_path):
    """Test that throttle_position sensor validates successfully."""
    yaml_text = BASE_YAML.format(
        sensors="throttle_position:\n      name: Throttle Position"
    )
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, "Expected validation to succeed for throttle_position"


def test_elm327_intake_air_temperature_sensor(tmp_path):
    """Test that intake_air_temperature sensor validates successfully."""
    yaml_text = BASE_YAML.format(
        sensors="intake_air_temperature:\n      name: Intake Air Temp"
    )
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, (
        "Expected validation to succeed for intake_air_temperature"
    )


def test_elm327_maf_rate_sensor(tmp_path):
    """Test that maf_rate sensor validates successfully."""
    yaml_text = BASE_YAML.format(sensors="maf_rate:\n      name: MAF Rate")
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, "Expected validation to succeed for maf_rate"


def test_elm327_fuel_level_sensor(tmp_path):
    """Test that fuel_level sensor validates successfully."""
    yaml_text = BASE_YAML.format(sensors="fuel_level:\n      name: Fuel Level")
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, "Expected validation to succeed for fuel_level"


def test_elm327_battery_voltage_sensor(tmp_path):
    """Test that battery_voltage sensor validates successfully."""
    yaml_text = BASE_YAML.format(
        sensors="battery_voltage:\n      name: Battery Voltage"
    )
    result = _load_and_validate(tmp_path, yaml_text)
    assert result is not None, "Expected validation to succeed for battery_voltage"


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
