"""Tests for the sensor component."""

import re


def _extract_packed_value(main_cpp, var_name):
    """Extract the third (packed) argument from a configure_entity_ call."""
    pattern = rf"{re.escape(var_name)}->configure_entity_\([^,]+,\s*\w+,\s*(\d+)\)"
    match = re.search(pattern, main_cpp)
    assert match, f"configure_entity_ call not found for {var_name}"
    return int(match.group(1))


def test_sensor_device_class_set(generate_main):
    """
    When the device_class of sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/sensor/test_sensor.yaml")

    # Then: device_class: voltage means packed value must be non-zero
    packed = _extract_packed_value(main_cpp, "s_1")
    assert packed != 0
