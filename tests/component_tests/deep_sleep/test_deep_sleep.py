"""Tests for the deep sleep component."""
import pytest
import esphome.config_validation as cv

def test_deep_sleep_setup(generate_main):
    """
    When the deep sleep is set in the yaml file, it should be registered in main
    """
    main_cpp = generate_main("tests/component_tests/deep_sleep/test_deep_sleep1.yaml")

    assert "deepsleep = new deep_sleep::DeepSleepComponent();" in main_cpp
    assert "App.register_component(deepsleep);" in main_cpp


def test_deep_sleep_sleep_duration(generate_main):
    """
    When deep sleep is configured with sleep duration, it should be set.
    """
    main_cpp = generate_main("tests/component_tests/deep_sleep/test_deep_sleep1.yaml")

    assert "deepsleep->set_sleep_duration(60000);" in main_cpp


def test_deep_sleep_run_duration_simple(generate_main):
    """
    When deep sleep is configured with run duration, it should be set.
    """
    main_cpp = generate_main("tests/component_tests/deep_sleep/test_deep_sleep1.yaml")

    assert "deepsleep->set_run_duration(10000);" in main_cpp


def test_deep_sleep_run_duration_dictionary(generate_main):
    """
    When deep sleep is configured with dictionary run duration, it should be set.
    """
    main_cpp = generate_main("tests/component_tests/deep_sleep/test_deep_sleep2.yaml")

    assert (
        "deepsleep->set_run_duration(deep_sleep::WakeupCauseToRunDuration{\n"
        "    .default_cause = 10000,\n"
        "    .touch_cause = 10000,\n"
        "    .gpio_cause = 30000,\n"
        "});"
    ) in main_cpp


def test_deep_sleep_single_perpin_definition(generate_main):
    """
    When deep sleep is configured with single per pin wakeup_pin_mode, it should work even on non-multi-wake-pin devices
    """
    main_cpp = generate_main("tests/component_tests/deep_sleep/test_deep_sleep2.yaml")
    print(main_cpp)

    assert (
        "esp32_esp32internalgpiopin = new esp32::ESP32InternalGPIOPin();\n"
        "esp32_esp32internalgpiopin->set_pin(::GPIO_NUM_2);"
    ) in main_cpp

    assert (
        "deepsleep->add_wakeup_pin(deep_sleep::WakeupPinItem{\n"
        "    .wakeup_pin = esp32_esp32internalgpiopin,\n"
        "    .wakeup_pin_mode = deep_sleep::WAKEUP_PIN_MODE_KEEP_AWAKE,\n"
        "});"
    ) in main_cpp


def test_deep_sleep_multi_perpin_definition(generate_main):
    """
    When deep sleep is configured with multiple per pin wakeup_pin_mode, it should be reflected in the output
    """
    main_cpp = generate_main("tests/component_tests/deep_sleep/test_deep_sleep3.yaml")

    assert (
        "esp32_esp32internalgpiopin = new esp32::ESP32InternalGPIOPin();\n"
        "esp32_esp32internalgpiopin->set_pin(::GPIO_NUM_2);"
    ) in main_cpp

    assert (
        "deep_sleep_deepsleepcomponent->add_wakeup_pin(deep_sleep::WakeupPinItem{\n"
        "    .wakeup_pin = esp32_esp32internalgpiopin,\n"
        "    .wakeup_pin_mode = deep_sleep::WAKEUP_PIN_MODE_INVERT_WAKEUP,\n"
        "});"
    ) in main_cpp

    assert (
        "esp32_esp32internalgpiopin_2 = new esp32::ESP32InternalGPIOPin();\n"
        "esp32_esp32internalgpiopin_2->set_pin(::GPIO_NUM_4);"
    ) in main_cpp

    assert (
        "deep_sleep_deepsleepcomponent->add_wakeup_pin(deep_sleep::WakeupPinItem{\n"
        "    .wakeup_pin = esp32_esp32internalgpiopin_2,\n"
        "    .wakeup_pin_mode = deep_sleep::WAKEUP_PIN_MODE_KEEP_AWAKE,\n"
        "});"
    ) in main_cpp


def test_deep_sleep_multi_pin_definition_on_unsupported(generate_main):
    """
    When deep sleep is configured with multiple pins on unsupported devices, it should fail
    """
    with pytest.raises(AttributeError):
        with pytest.raises(cv.Invalid) as excinfo:
            main_cpp = generate_main(
                "tests/component_tests/deep_sleep/test_deep_sleep4.yaml"
            )
        assert "Your board only supports wake from a single pin" in str(excinfo.value)
