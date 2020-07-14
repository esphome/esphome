""" Tests for the binary sensor component """


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
