"""Tests for the text sensor component."""

from tests.component_tests.helpers import INTERNAL_BIT, extract_packed_value


def test_text_sensor_is_setup(generate_main):
    """
    When the text is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text_sensor/test_text_sensor.yaml")

    # Then
    assert "static template_::TemplateTextSensor *const" in main_cpp
    assert ") template_::TemplateTextSensor();" in main_cpp
    assert "App.register_text_sensor" in main_cpp


def test_text_sensor_sets_mandatory_fields(generate_main):
    """
    When the mandatory fields are set in the yaml, they should be set in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text_sensor/test_text_sensor.yaml")

    # Then
    assert 'App.register_text_sensor(ts_1, "Template Text Sensor 1",' in main_cpp
    assert 'App.register_text_sensor(ts_2, "Template Text Sensor 2",' in main_cpp
    assert 'App.register_text_sensor(ts_3, "Template Text Sensor 3",' in main_cpp


def test_text_sensor_config_value_internal_set(generate_main):
    """
    Test that the "internal" config value is correctly set
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text_sensor/test_text_sensor.yaml")

    # Then: ts_2 has internal: true, ts_3 has internal: false
    assert extract_packed_value(main_cpp, "ts_2") & INTERNAL_BIT != 0
    assert extract_packed_value(main_cpp, "ts_3") & INTERNAL_BIT == 0


def test_text_sensor_device_class_set(generate_main):
    """
    When the device_class of text_sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/text_sensor/test_text_sensor.yaml")

    # Then: ts_2 has device_class: timestamp, ts_3 has device_class: date
    # so their packed values must be non-zero
    packed_ts_2 = extract_packed_value(main_cpp, "ts_2")
    assert packed_ts_2 != 0
    packed_ts_3 = extract_packed_value(main_cpp, "ts_3")
    assert packed_ts_3 != 0
