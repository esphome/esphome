"""Tests for the chargery_bms component."""


def test_sensor_device_class_set(generate_main):
    """
    When the device_class of chargery_bms is set in the yaml file, it should be registered in main
    """
    # Given

    # When
    main_cpp = generate_main(
        "tests/component_tests/chargery_bms/test_chargery_bms.yaml"
    )

    # Then
    assert 'sensor->set_device_class("voltage");' in main_cpp
