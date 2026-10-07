"""Tests for the sensor component."""

from tests.component_tests.helpers import extract_packed_value


def test_sensor_device_class_set(generate_main):
    """
    When the device_class of sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/sensor/test_sensor.yaml")

    # Then: device_class: voltage means packed value must be non-zero
    packed = extract_packed_value(main_cpp, "s_1")
    assert packed != 0
