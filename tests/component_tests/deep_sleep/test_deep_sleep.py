"""Tests for the deep sleep component."""

from collections.abc import Callable
from pathlib import Path
import re

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


def test_deep_sleep_empty_wakeup_pin_list_rejected() -> None:
    """An empty wakeup_pin list is rejected at validation instead of crashing codegen."""
    with pytest.raises(cv.Invalid, match="at least one entry"):
        deep_sleep.validate_wakeup_pin([])


def test_deep_sleep_bk72xx_wakeup_pin_mode_without_wakeup_pin_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """On BK72xx, a top-level wakeup_pin_mode with no wakeup_pin is rejected at validation."""
    set_core_config(PlatformFramework.BK72XX_ARDUINO)
    config = {deep_sleep.CONF_WAKEUP_PIN_MODE: "KEEP_AWAKE"}
    with pytest.raises(cv.Invalid, match="requires 'wakeup_pin'"):
        deep_sleep.CONFIG_SCHEMA(config)


def test_deep_sleep_esp32_wakeup_pin_mode_without_wakeup_pin_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """On ESP32 the same mode-only config is rejected too, instead of being silently dead."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = {deep_sleep.CONF_WAKEUP_PIN_MODE: "KEEP_AWAKE"}
    with pytest.raises(cv.Invalid, match="requires 'wakeup_pin'"):
        deep_sleep.CONFIG_SCHEMA(config)


@pytest.mark.parametrize(
    "platform", [PlatformFramework.ESP8266_ARDUINO, PlatformFramework.NRF52_ZEPHYR]
)
def test_deep_sleep_wakeup_pin_rejected_on_unsupported_platform(
    set_core_config: SetCoreConfigCallable, platform: PlatformFramework
) -> None:
    """wakeup_pin is rejected where the C++ component has no wakeup pin support."""
    set_core_config(platform)
    with pytest.raises(cv.Invalid, match="only available on"):
        deep_sleep.CONFIG_SCHEMA({CONF_WAKEUP_PIN: "GPIO4"})


def test_deep_sleep_esp32_multiple_wakeup_pins_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """ESP32 accepts only one wakeup_pin entry."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = {CONF_WAKEUP_PIN: [{"pin": "GPIO4"}, {"pin": "GPIO5"}]}
    with pytest.raises(cv.Invalid, match="ESP32 does not support multiple") as exc:
        deep_sleep.validate_config(config)
    assert exc.value.path == [CONF_WAKEUP_PIN]


def test_deep_sleep_bk72xx_global_wakeup_pin_mode_with_multiple_pins_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """On BK72xx, a top-level wakeup_pin_mode cannot be applied to several pins."""
    set_core_config(PlatformFramework.BK72XX_ARDUINO)
    config = {
        CONF_WAKEUP_PIN: [{"pin": "GPIO12"}, {"pin": "GPIO13"}],
        deep_sleep.CONF_WAKEUP_PIN_MODE: "KEEP_AWAKE",
    }
    with pytest.raises(cv.Invalid, match="define it per pin") as exc:
        deep_sleep.validate_config(config)
    assert exc.value.path == [deep_sleep.CONF_WAKEUP_PIN_MODE]


def test_deep_sleep_esp32_nested_wakeup_pin_mode(
    generate_main: Callable[[str | Path], str],
) -> None:
    """On ESP32, a wakeup_pin_mode nested under the pin entry is applied, not dropped."""
    main_cpp = generate_main(
        "tests/component_tests/deep_sleep/test_deep_sleep_nested_wakeup_pin_mode.yaml"
    )

    assert "deepsleep->set_wakeup_pin(" in main_cpp
    assert "deepsleep->set_wakeup_pin_mode(" in main_cpp
    assert "WAKEUP_PIN_MODE_KEEP_AWAKE" in main_cpp


def test_deep_sleep_esp32_wakeup_pin_mode_at_both_levels_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """On ESP32, wakeup_pin_mode at the top level and under the pin entry is an error."""
    set_core_config(PlatformFramework.ESP32_IDF)
    config = {
        CONF_WAKEUP_PIN: [
            {"pin": "GPIO4", deep_sleep.CONF_WAKEUP_PIN_MODE: "KEEP_AWAKE"}
        ],
        deep_sleep.CONF_WAKEUP_PIN_MODE: "INVERT_WAKEUP",
    }
    with pytest.raises(cv.Invalid, match="not both") as exc:
        deep_sleep.validate_config(config)
    assert exc.value.path == [CONF_WAKEUP_PIN, 0, deep_sleep.CONF_WAKEUP_PIN_MODE]


ENTER_YAML = "tests/component_tests/deep_sleep/test_deep_sleep_enter.yaml"


def test_deep_sleep_enter_without_duration(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A bare deep_sleep.enter only begins a manual sleep with the configured duration."""
    main_cpp = generate_main(ENTER_YAML)

    assert (
        "new(deep_sleep_enterdeepsleepaction_id) "
        "deep_sleep::EnterDeepSleepAction<>(deepsleep);\n"
        "new(automation_id) Automation<>(sleep_now);"
    ) in main_cpp


def test_deep_sleep_enter_constant_duration(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A constant sleep_duration is set in milliseconds."""
    main_cpp = generate_main(ENTER_YAML)

    assert (
        "deep_sleep_enterdeepsleepaction_id_2->set_sleep_duration([]() -> uint32_t {\n"
        "    return 3600000;\n"
        "});"
    ) in main_cpp


def test_deep_sleep_enter_lambda_duration(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A lambda sleep_duration is computed from the script parameters."""
    main_cpp = generate_main(ENTER_YAML)

    assert re.search(
        r"new\(deep_sleep_enterdeepsleepaction_id_3\) "
        r"deep_sleep::EnterDeepSleepAction<uint32_t>\(deepsleep\);\n"
        r"deep_sleep_enterdeepsleepaction_id_3->set_sleep_duration\("
        r"\[\]\(uint32_t seconds\) -> uint32_t \{\n(?:    #line [^\n]*\n)?"
        r"    return seconds \* 1000;\n",
        main_cpp,
    )


@pytest.mark.parametrize(
    ("action_id", "body"),
    [
        ("deep_sleep_enterdeepsleepaction_id_4", "seconds *= 1000; return seconds;"),
        ("deep_sleep_enterdeepsleepaction_id_5", "return seconds *= 1000;"),
    ],
)
def test_deep_sleep_enter_lambda_takes_args_by_value(
    generate_main: Callable[[str | Path], str], action_id: str, body: str
) -> None:
    """The sleep_duration lambda gets the script parameters by value, so it may modify them."""
    main_cpp = generate_main(ENTER_YAML)

    assert re.search(
        action_id + r"->set_sleep_duration\("
        r"\[\]\(uint32_t seconds\) -> uint32_t \{\n(?:    #line [^\n]*\n)?    "
        + re.escape(body)
        + "\n",
        main_cpp,
    )


def test_deep_sleep_enter_until(
    generate_main: Callable[[str | Path], str],
) -> None:
    """The until and time_id keys set the wake time of day and the clock to read it from."""
    main_cpp = generate_main(ENTER_YAML)

    assert (
        "deep_sleep_enterdeepsleepaction_id_6->set_until(6, 30, 0);\n"
        "deep_sleep_enterdeepsleepaction_id_6->set_time(sntp_time);"
    ) in main_cpp
