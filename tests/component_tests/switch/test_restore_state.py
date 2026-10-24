"""Tests for switch `initial_state` / `restore_state` (including `storage`)."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components import switch
from esphome.components.libretiny.const import (
    FAMILY_BK7231T,
    FAMILY_BK7251,
    FAMILY_LN882H,
    FAMILY_RTL8720C,
    KEY_FAMILY,
    KEY_LIBRETINY,
)
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from esphome.core import CORE
from tests.component_tests.types import SetCoreConfigCallable

GATE = "USE_SWITCH_STORAGE"
ESP32_RTC = "USE_ESP32_RTC_PREFERENCES"


def _defines() -> set[str]:
    return {define.name for define in CORE.defines}


@pytest.mark.parametrize(
    ("switch_id", "mode"),
    [
        ("keep_short", "SWITCH_RESTORE_DEFAULT_OFF"),
        ("all_short", "SWITCH_RESTORE_DEFAULT_OFF"),
        ("invert_on", "SWITCH_RESTORE_INVERTED_DEFAULT_ON"),
        ("initial_on", "SWITCH_ALWAYS_ON"),
        ("none_on", "SWITCH_ALWAYS_ON"),
        ("initial_only", "SWITCH_ALWAYS_ON"),
        ("legacy", "SWITCH_RESTORE_DEFAULT_OFF"),
        ("plain", "SWITCH_ALWAYS_OFF"),
    ],
)
def test_restore_state_maps_to_a_restore_mode(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    switch_id: str,
    mode: str,
) -> None:
    """Every initial_state / restore_state combination becomes one existing restore mode."""
    main_cpp = generate_main(component_config_path("restore_state_esp32.yaml"))

    assert f"{switch_id}->set_restore_mode(switch_::{mode});" in main_cpp


def test_storage_is_passed_only_when_set(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("restore_state_esp32.yaml"))

    assert (
        "keep_rtc->set_restore_mode(static_cast<switch_::SwitchRestoreMode>("
        "switch_::SWITCH_RESTORE_DEFAULT_ON | switch_::RESTORE_MODE_STORAGE_RTC_MASK));"
    ) in main_cpp
    assert (
        "keep_flash->set_restore_mode(static_cast<switch_::SwitchRestoreMode>("
        "switch_::SWITCH_RESTORE_DEFAULT_OFF | switch_::RESTORE_MODE_STORAGE_FLASH_MASK));"
    ) in main_cpp
    assert (
        "keep_short->set_restore_mode(switch_::SWITCH_RESTORE_DEFAULT_OFF);" in main_cpp
    )
    assert GATE in _defines()
    assert ESP32_RTC in _defines()


def test_nothing_new_emitted_without_restore_state(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Configs using only restore_mode generate exactly what they did before."""
    main_cpp = generate_main(component_config_path("restore_state_absent_esp32.yaml"))

    assert "RESTORE_MODE_STORAGE_" not in main_cpp
    assert GATE not in _defines()
    assert ESP32_RTC not in _defines()


def test_rtc_applies_per_switch_on_esp8266_with_restore_from_flash(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("restore_state_esp8266.yaml"))

    assert (
        "keep_rtc->set_restore_mode(static_cast<switch_::SwitchRestoreMode>("
        "switch_::SWITCH_RESTORE_DEFAULT_ON | switch_::RESTORE_MODE_STORAGE_RTC_MASK));"
    ) in main_cpp
    assert ESP32_RTC not in _defines()


def _validate(config: dict, *, default_restore_mode: str = cv.UNDEFINED) -> dict:
    schema = switch.switch_schema(
        switch.Switch, default_restore_mode=default_restore_mode
    )
    return schema({"id": "test_switch", **config})


@pytest.fixture
def esp8266(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)


@pytest.mark.usefixtures("esp8266")
def test_restore_mode_and_restore_state_are_exclusive() -> None:
    with pytest.raises(cv.Invalid, match="restore"):
        _validate({"restore_mode": "RESTORE_DEFAULT_OFF", "restore_state": "all"})


@pytest.mark.usefixtures("esp8266")
def test_initial_state_rejected_with_restore_mode() -> None:
    """restore_mode already decides the boot state, so initial_state would be ignored."""
    with pytest.raises(cv.Invalid, match="initial_state"):
        _validate({"restore_mode": "ALWAYS_OFF", "initial_state": True})


@pytest.mark.usefixtures("esp8266")
def test_storage_rejected_when_nothing_is_restored() -> None:
    with pytest.raises(cv.Invalid, match="only applies"):
        _validate({"restore_state": {"state": "INITIAL", "storage": "rtc"}})


@pytest.mark.usefixtures("esp8266")
@pytest.mark.parametrize("value", ["RESTORE", "ON", "OFF", "KEEP", "INVERT", "INITIAL"])
def test_unsupported_restore_state_values_rejected(value: object) -> None:
    # Only light's shorthands (all, none) are accepted as a string
    with pytest.raises(cv.Invalid, match="expected 'all', 'none' or a mapping"):
        _validate({"restore_state": value})


@pytest.mark.usefixtures("esp8266")
@pytest.mark.parametrize("state", [True, False, "ON", "off"])
def test_restore_state_on_off_explains_what_to_use(state: object) -> None:
    # Valid on light, so people will try it; YAML turns ON into True
    with pytest.raises(
        cv.Invalid, match="initial_state: ON' \\(or OFF\\) with 'restore_state: none'"
    ):
        _validate({"restore_state": {"state": state}})


@pytest.mark.parametrize(
    ("platform", "family"),
    [
        (PlatformFramework.BK72XX_ARDUINO, FAMILY_BK7251),
        (PlatformFramework.RTL87XX_ARDUINO, FAMILY_RTL8720C),
        (PlatformFramework.LN882X_ARDUINO, FAMILY_LN882H),
        (PlatformFramework.RP2040_ARDUINO, None),
        (PlatformFramework.NRF52_ZEPHYR, None),
        (PlatformFramework.HOST_NATIVE, None),
    ],
)
def test_rtc_rejected_without_an_rtc_backend(
    set_core_config: SetCoreConfigCallable,
    platform: PlatformFramework,
    family: str | None,
) -> None:
    """These preferences backends ignore in_flash, so the state would silently stay in flash."""
    set_core_config(platform)
    if family is not None:
        CORE.data[KEY_LIBRETINY] = {KEY_FAMILY: family}
    with pytest.raises(cv.Invalid, match="'rtc' storage is not supported"):
        _validate({"restore_state": {"state": "KEEP", "storage": "rtc"}})


def test_rtc_accepted_on_a_beken_chip_with_the_emulation(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.BK72XX_ARDUINO)
    CORE.data[KEY_LIBRETINY] = {KEY_FAMILY: FAMILY_BK7231T}
    _validate({"restore_state": {"state": "KEEP", "storage": "rtc"}})


@pytest.mark.usefixtures("esp8266")
def test_rtc_accepted_on_esp8266() -> None:
    _validate({"restore_state": {"state": "KEEP", "storage": "rtc"}})


@pytest.mark.usefixtures("esp8266")
def test_platform_default_restore_mode_applies_only_when_nothing_is_set() -> None:
    """Platforms such as haier default to DISABLED; restore_state must still be usable there."""
    assert _validate({}, default_restore_mode="DISABLED")["restore_mode"] == "DISABLED"
    configured = _validate({"restore_state": "all"}, default_restore_mode="DISABLED")
    assert "restore_mode" not in configured


@pytest.mark.usefixtures("esp8266")
@pytest.mark.parametrize(
    ("default_restore_mode", "initial_state", "expected"),
    [
        # A restoring default keeps restoring; initial_state only replaces its fallback
        ("RESTORE_DEFAULT_ON", False, "RESTORE_DEFAULT_OFF"),
        ("RESTORE_DEFAULT_OFF", True, "RESTORE_DEFAULT_ON"),
        ("RESTORE_INVERTED_DEFAULT_OFF", True, "RESTORE_INVERTED_DEFAULT_ON"),
        # A default that restores nothing starts in initial_state
        ("ALWAYS_OFF", True, "ALWAYS_ON"),
        ("DISABLED", True, "ALWAYS_ON"),
    ],
)
def test_initial_state_alone_keeps_a_restoring_platform_default(
    default_restore_mode: str, initial_state: bool, expected: str
) -> None:
    config = _validate(
        {"initial_state": initial_state}, default_restore_mode=default_restore_mode
    )
    assert "restore_mode" not in config
    assert "storage" not in config.get("restore_state", {})
    assert switch._resolve_restore_mode(config) is switch.RESTORE_MODES[expected]
