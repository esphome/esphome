"""Tests for the text sensor component."""


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
    assert 'ts_1->set_name("Template Text Sensor 1");' in main_cpp
    assert 'ts_2->set_name("Template Text Sensor 2");' in main_cpp
    assert 'ts_3->set_name("Template Text Sensor 3");' in main_cpp


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

    # Then
    assert 'ts_2->set_device_class("timestamp");' in main_cpp
    assert 'ts_3->set_device_class("date");' in main_cpp
