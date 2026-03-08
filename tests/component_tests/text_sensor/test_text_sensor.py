"""Tests for the text sensor component."""

import re


def _extract_packed_value(main_cpp, var_name):
    """Extract the third (packed) argument from a configure_entity_ call."""
    pattern = rf"{re.escape(var_name)}->configure_entity_\([^,]+,\s*\w+,\s*(\d+)\)"
    match = re.search(pattern, main_cpp)
    assert match, f"configure_entity_ call not found for {var_name}"
    return int(match.group(1))


def test_text_sensor_is_setup(generate_main):
    """
    When the text is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text_sensor/test_text_sensor.yaml")

    # Then
    assert "new template_::TemplateTextSensor();" in main_cpp
    assert "App.register_text_sensor" in main_cpp


def test_text_sensor_sets_mandatory_fields(generate_main):
    """
    When the mandatory fields are set in the yaml, they should be set in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text_sensor/test_text_sensor.yaml")

    # Then
    assert 'ts_1->configure_entity_("Template Text Sensor 1",' in main_cpp
    assert 'ts_2->configure_entity_("Template Text Sensor 2",' in main_cpp
    assert 'ts_3->configure_entity_("Template Text Sensor 3",' in main_cpp


def test_text_sensor_config_value_internal_set(generate_main):
    """
    Test that the "internal" config value is correctly set
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text_sensor/test_text_sensor.yaml")

    # Then
    assert "ts_2->set_internal(true);" in main_cpp
    assert "ts_3->set_internal(false);" in main_cpp


def test_text_sensor_device_class_set(generate_main):
    """
    When the device_class of text_sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text_sensor/test_text_sensor.yaml")

    # Then: ts_2 has device_class: timestamp, ts_3 has device_class: date
    # so their packed values must be non-zero
    packed_ts_2 = _extract_packed_value(main_cpp, "ts_2")
    assert packed_ts_2 != 0
    packed_ts_3 = _extract_packed_value(main_cpp, "ts_3")
    assert packed_ts_3 != 0
