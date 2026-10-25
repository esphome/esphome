"""Tests for the deep sleep component."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components import deep_sleep
from esphome.components.zephyr.variants import VARIANTS
from esphome.const import CONF_WAKEUP_PIN, PlatformFramework
from esphome.core import CORE

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


def test_deep_sleep_zephyr_wakeup_pin(
    generate_main: Callable[[str | Path], str],
) -> None:
    """platform: zephyr arms the pin before sys_poweroff() and keeps the mode."""
    main_cpp = generate_main(
        "tests/component_tests/deep_sleep/test_deep_sleep_zephyr_wakeup_pin.yaml"
    )

    assert "deep_sleep_deepsleepcomponent_id->set_wakeup_pin(" in main_cpp
    assert "set_wakeup_pin_mode(deep_sleep::WAKEUP_PIN_MODE_INVERT_WAKEUP)" in main_cpp
    assert any(d.name == "USE_DEEP_SLEEP_ZEPHYR_WAKEUP_PIN" for d in CORE.defines)


def _set_zephyr_variant(set_core_config: SetCoreConfigCallable, variant: str) -> None:
    set_core_config(
        PlatformFramework.ZEPHYR_ZEPHYR,
        platform_data={"variant": variant, "family": VARIANTS[variant].family},
    )


@pytest.mark.parametrize(
    "variant", ["EFR32MG24", "SIWX917", "RP2040", "RP2350", "RA4M1"]
)
def test_deep_sleep_zephyr_rejected_without_poweroff(
    set_core_config: SetCoreConfigCallable, variant: str
) -> None:
    _set_zephyr_variant(set_core_config, variant)
    with pytest.raises(cv.Invalid, match="deep_sleep is not supported"):
        deep_sleep.validate_config({})


@pytest.mark.parametrize("variant", ["ESP32C6", "NRF52", "STM32F4"])
def test_deep_sleep_zephyr_allowed_with_poweroff(
    set_core_config: SetCoreConfigCallable, variant: str
) -> None:
    _set_zephyr_variant(set_core_config, variant)
    assert deep_sleep.validate_config({}) == {}


_POWEROFF_FAMILIES = {"esp32", "nordic", "stm32"}


@pytest.mark.parametrize("variant", sorted(VARIANTS))
def test_deep_sleep_zephyr_verdict_follows_the_family_for_every_variant(
    set_core_config: SetCoreConfigCallable, variant: str
) -> None:
    """A new variant must land on the right side of the family rule, not go untested."""
    _set_zephyr_variant(set_core_config, variant)
    if VARIANTS[variant].family in _POWEROFF_FAMILIES:
        assert deep_sleep.validate_config({}) == {}
    else:
        with pytest.raises(cv.Invalid, match="deep_sleep is not supported"):
            deep_sleep.validate_config({})
