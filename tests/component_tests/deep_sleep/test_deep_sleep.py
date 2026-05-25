"""Tests for the deep sleep component."""


def test_deep_sleep_setup(generate_main):
    """
    When the deep sleep is set in the yaml file, it should be registered in main
    """
    main_cpp = generate_main("tests/component_tests/deep_sleep/test_deep_sleep1.yaml")

    assert (
        "static deep_sleep::DeepSleepComponent *const deepsleep = reinterpret_cast<deep_sleep::DeepSleepComponent *>(deep_sleep__deepsleep__pstorage);"
        in main_cpp
    )
    assert "new(deepsleep) deep_sleep::DeepSleepComponent();" in main_cpp
    assert "App.register_component_(deepsleep, " in main_cpp


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


def test_deep_sleep_esp32_nested_wakeup_pin_mode(generate_main):
    """
    On ESP32, a wakeup_pin_mode nested under a single-pin list entry should be
    hoisted to the top level so it actually takes effect rather than being
    silently dropped by the codegen.
    """
    main_cpp = generate_main("tests/component_tests/deep_sleep/test_deep_sleep3.yaml")

    assert "deepsleep->set_wakeup_pin(" in main_cpp
    assert "deepsleep->set_wakeup_pin_mode(" in main_cpp
    assert "WAKEUP_PIN_MODE_KEEP_AWAKE" in main_cpp
