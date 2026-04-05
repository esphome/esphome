"""Tests for the sensor component."""

from tests.component_tests.helpers import extract_packed_value


def test_sensor_device_class_set(generate_main):
    """
    When the device_class of sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/sensor/test_sensor.yaml")

    # Then
    assert 's_1->configure_entity_("test s1", 1997041708, 257);' in main_cpp
    assert (
        "threshold_id->set_upper_threshold([]() -> float {\n    return s_1->state;"
        in main_cpp
    )
    # Then: device_class: voltage means packed value must be non-zero
    packed = extract_packed_value(main_cpp, "s_1")
    assert packed != 0
