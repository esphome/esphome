"""Tests for the sensor component."""


def test_sensor_device_class_set(generate_main):
    """
    When the device_class of sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/sensor/test_sensor.yaml")

    # Then
    assert 's_1->set_device_class("voltage");' in main_cpp
    assert (
        "threshold_id->set_upper_threshold([]() -> float {\n    return s_1->state;"
        in main_cpp
    )
