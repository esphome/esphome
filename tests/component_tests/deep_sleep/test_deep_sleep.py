"""Tests for the deep sleep component."""

import pytest

from esphome import config_validation as cv
from esphome.components import deep_sleep
from esphome.const import CONF_WAKEUP_PIN, PlatformFramework

from ..types import SetCoreConfigCallable


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


def test_deep_sleep_on_wake_trigger(generate_main):
    """
    When deep sleep is configured with a component-level on_wake automation,
    a WakeTrigger component should be registered with the wakeup cause as
    the automation argument.
    """
    main_cpp = generate_main("tests/component_tests/deep_sleep/test_deep_sleep3.yaml")

    assert "deep_sleep::WakeTrigger();" in main_cpp
    assert "Automation<deep_sleep::WakeupCause>" in main_cpp


def test_deep_sleep_ext1_on_wake_triggers(generate_main):
    """
    Each esp32_ext1_wakeup pin with an on_wake automation should get its own
    Ext1WakeTrigger with the pin number, and all pins (including the legacy
    bare-pin shorthand) should contribute to the ext1 wakeup mask.
    """
    main_cpp = generate_main("tests/component_tests/deep_sleep/test_deep_sleep3.yaml")

    assert "deep_sleep::Ext1WakeTrigger(2);" in main_cpp
    assert "deep_sleep::Ext1WakeTrigger(4);" in main_cpp
    # GPIO13 has no on_wake, so no trigger is created for it
    assert "deep_sleep::Ext1WakeTrigger(13)" not in main_cpp
    # mask covers GPIO2, GPIO4 and GPIO13
    assert ".mask = 8212," in main_cpp


def test_deep_sleep_no_on_wake_no_triggers(generate_main):
    """
    Without any on_wake automations, no wake trigger code should be generated.
    """
    main_cpp = generate_main("tests/component_tests/deep_sleep/test_deep_sleep1.yaml")

    assert "WakeTrigger" not in main_cpp


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


def test_deep_sleep_bk72xx_wakeup_pin_mode_at_both_levels_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """On BK72xx, wakeup_pin_mode at the top level and under the pin entry is an error."""
    set_core_config(PlatformFramework.BK72XX_ARDUINO)
    config = {
        CONF_WAKEUP_PIN: [
            {"pin": "GPIO12", deep_sleep.CONF_WAKEUP_PIN_MODE: "KEEP_AWAKE"}
        ],
        deep_sleep.CONF_WAKEUP_PIN_MODE: "INVERT_WAKEUP",
    }
    with pytest.raises(cv.Invalid, match="not both"):
        deep_sleep.validate_config(config)


def test_deep_sleep_bk72xx_top_level_wakeup_pin_mode_moved_onto_single_pin(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """On BK72xx, a top-level wakeup_pin_mode is moved onto the only pin entry."""
    set_core_config(PlatformFramework.BK72XX_ARDUINO)
    config = {
        CONF_WAKEUP_PIN: [{"pin": "GPIO12"}],
        deep_sleep.CONF_WAKEUP_PIN_MODE: "INVERT_WAKEUP",
    }
    result = deep_sleep.validate_config(config)

    assert deep_sleep.CONF_WAKEUP_PIN_MODE not in result
    assert (
        result[CONF_WAKEUP_PIN][0][deep_sleep.CONF_WAKEUP_PIN_MODE] == "INVERT_WAKEUP"
    )
