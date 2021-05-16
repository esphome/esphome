"""Tests for the binary sensor component."""


def test_esp32_pulse_counter_is_setup(generate_main):
    """
    Test the default sensor setup.
    """
    # Given

    # When
    main_cpp = generate_main(
        "tests/component_tests/esp32_pulse_counter/tc01.yaml"
    )

    # Then
    assert 'esp32_pulse_counter_esp32pulsecountersensor = new esp32_pulse_counter::ESP32PulseCounterSensor();' in main_cpp
    assert 'new esp32_pulse_counter::ESP32PulseCounterSensor();' in main_cpp
    assert 'App.register_component(esp32_pulse_counter_esp32pulsecountersensor);' in main_cpp
    assert 'esp32_pulse_counter_esp32pulsecountersensor->set_name("PC1");' in main_cpp
    assert 'esp32_pulse_counter_esp32pulsecountersensor->set_unit_of_measurement("pulses/min");' in main_cpp
    assert 'esp32_pulse_counter_esp32pulsecountersensor->set_icon("mdi:pulse");' in main_cpp
    assert 'esp32_pulse_counter_esp32pulsecountersensor->set_accuracy_decimals(2);' in main_cpp
    assert 'esp32_pulse_counter_esp32pulsecountersensor->set_force_update(false);' in main_cpp
    assert 'esp32_pulse_counter_esp32pulsecountersensor->set_pin(new GPIOPin(1, INPUT, false));' in main_cpp
    assert 'esp32_pulse_counter_esp32pulsecountersensor->set_rising_edge_mode(esp32_pulse_counter::PULSE_COUNTER_INCREMENT);' in main_cpp
    assert 'esp32_pulse_counter_esp32pulsecountersensor->set_falling_edge_mode(esp32_pulse_counter::PULSE_COUNTER_DISABLE);' in main_cpp
    assert 'esp32_pulse_counter_esp32pulsecountersensor->set_filter_us(13);' in main_cpp
    assert "esp32_pulse_counter_esp32pulsecountersensor_2->set_filter_us(10);" in main_cpp
    assert "esp32_pulse_counter_esp32pulsecountersensor_3->set_update_interval(10000);" in main_cpp


# def test_esp32_pulse_counter_internal_filter_is_setup(generate_main):
#     """
#     Test if the internal_filter get set correct.
#     """
#     # Given

#     # When
#     main_cpp = generate_main(
#         "tests/component_tests/esp32_pulse_counter/tc01.yaml"
#     )

#     # Then
#     assert "esp32_pulse_counter_esp32pulsecountersensor_2->set_filter_us(10);" in main_cpp

# def test_esp32_pulse_counter_update_interval_is_setup(generate_main):
#     """
#     Test if the update_interval get set correct.
#     """
#     # Given

#     # When
#     main_cpp = generate_main(
#         "tests/component_tests/esp32_pulse_counter/tc01.yaml"
#     )

#     # Then
#     assert "esp32_pulse_counter_esp32pulsecountersensor_3->set_update_interval(10000);" in main_cpp

# def test_esp32_pulse_counter_more_than_max_instances(generate_main):
#     """
#     Test if the update_interval get set correct.
#     """
#     # Given

#     # When
#     main_cpp = generate_main(
#         "tests/component_tests/esp32_pulse_counter/tc02.yaml"
#     )

#     # Then
#     assert "esp32_pulse_counter_esp32pulsecountersensor_3->set_update_interval(10000);" in main_cpp
