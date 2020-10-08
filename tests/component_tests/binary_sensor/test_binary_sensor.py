""" Tests for the binary sensor component """


def test_binary_sensor_is_setup(generate_main):
    """
    When the binary sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/binary_sensor/test_binary_sensor.yaml")

    # Then
    assert "new gpio::GPIOBinarySensor();" in main_cpp
    assert "App.register_binary_sensor" in main_cpp


def test_binary_sensor_sets_mandatory_fields(generate_main):
    """
    When the mandatory fields are set in the yaml, they should be set in main
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/binary_sensor/test_binary_sensor.yaml")

    # Then
    assert "bs_1->set_name(\"test bs1\");" in main_cpp
    assert "bs_1->set_pin(new GPIOPin" in main_cpp


def test_binary_sensor_config_value_internal_set(generate_main):
    """
    Test that the "internal" config value is correctly set
    """
    # Given

    # When
    main_cpp = generate_main("tests/component_tests/binary_sensor/test_binary_sensor.yaml")

    # Then
    assert "bs_1->set_internal(true);" in main_cpp
    assert "bs_2->set_internal(false);" in main_cpp
