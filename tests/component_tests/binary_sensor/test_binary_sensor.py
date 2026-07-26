"""Tests for the binary sensor component."""

from tests.component_tests.helpers import INTERNAL_BIT, extract_packed_value


def test_binary_sensor_is_setup(generate_main):
    """
    When the binary sensor is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main(
        "tests/component_tests/binary_sensor/test_binary_sensor.yaml"
    )

    # Then
    assert "static gpio::GPIOBinarySensor *const" in main_cpp
    assert "App.register_binary_sensor" in main_cpp


def test_binary_sensor_sets_mandatory_fields(generate_main):
    """
    When the mandatory fields are set in the yaml, they should be set in main
    """
    # Given

    # When
    main_cpp = generate_main(
        "tests/component_tests/binary_sensor/test_binary_sensor.yaml"
    )

    # Then
    assert 'App.register_binary_sensor(bs_1, "test bs1",' in main_cpp
    assert "bs_1->set_pin(" in main_cpp


def test_binary_sensor_config_value_internal_set(generate_main):
    """
    Test that the "internal" config value is correctly set
    """
    # Given

    # When
    main_cpp = generate_main(
        "tests/component_tests/binary_sensor/test_binary_sensor.yaml"
    )

    # Then: bs_1 has internal: true, bs_2 has internal: false
    assert extract_packed_value(main_cpp, "bs_1") & INTERNAL_BIT != 0
    assert extract_packed_value(main_cpp, "bs_2") & INTERNAL_BIT == 0


def test_binary_sensor_config_value_use_raw_set(generate_main):
    """
    Test that the "use_raw" config value is correctly set
    """
    # Given

    # When
    main_cpp = generate_main(
        "tests/component_tests/binary_sensor/test_binary_sensor.yaml"
    )

    # Then
    assert "bs_3->set_use_raw(true);" in main_cpp
    assert "bs_4->set_use_raw(false);" in main_cpp
