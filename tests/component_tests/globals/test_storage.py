"""Tests for the globals storage option."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components import globals as globals_component
from esphome.components.esp32 import KEY_VARIANT, VARIANT_ESP32, VARIANT_ESP32C2
from esphome.components.esp8266.const import (
    KEY_ESP8266,
    KEY_PREFERENCES,
    preference_words,
)
from esphome.const import (
    CONF_ID,
    CONF_RESTORE_VALUE,
    CONF_STORAGE,
    CONF_TYPE,
    PlatformFramework,
)
from esphome.core import CORE
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _validate(**extra: object) -> ConfigType:
    return globals_component.CONFIG_SCHEMA(
        {CONF_ID: "my_global", CONF_TYPE: "int", **extra}
    )


def _define_names() -> set[str]:
    return {define.name for define in CORE.defines}


@pytest.mark.parametrize(
    ("platform_framework", "platform_data"),
    [
        (PlatformFramework.ESP32_IDF, {KEY_VARIANT: VARIANT_ESP32C2}),
        (PlatformFramework.RP2040_ARDUINO, None),
    ],
)
def test_rtc_rejected_without_rtc_memory(
    set_core_config: SetCoreConfigCallable,
    platform_framework: PlatformFramework,
    platform_data: ConfigType | None,
) -> None:
    set_core_config(platform_framework, platform_data=platform_data)
    with pytest.raises(cv.Invalid, match="not supported on this platform"):
        _validate(**{CONF_RESTORE_VALUE: True, CONF_STORAGE: "rtc"})


@pytest.mark.parametrize("extra", [{}, {CONF_RESTORE_VALUE: False}])
def test_storage_rejected_without_restore_value(
    set_core_config: SetCoreConfigCallable, extra: ConfigType
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32}
    )
    with pytest.raises(cv.Invalid, match="'storage' needs 'restore_value: true'"):
        _validate(**extra, **{CONF_STORAGE: "rtc"})


def test_storage_generates_template_argument(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("globals_storage_test.yaml"))
    assert (
        "globals::RestoringGlobalsComponent<int, globals::Storage::STORAGE_RTC>"
        in main_cpp
    )
    assert (
        "globals::RestoringGlobalStringComponent<std::string, 16, "
        "globals::Storage::STORAGE_FLASH>" in main_cpp
    )
    assert "USE_ESP32_RTC_PREFERENCES" in _define_names()


# On the ESP8266 the omitted form must stay the two-argument call so restore_from_flash keeps deciding
@pytest.mark.parametrize("config", ["globals_test.yaml", "globals_esp8266_test.yaml"])
def test_omitted_storage_keeps_platform_default(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config: str,
) -> None:
    main_cpp = generate_main(component_config_path(config))
    assert (
        "new(my_restoring_int) globals::RestoringGlobalsComponent<int>(8)" in main_cpp
    )
    assert "globals::Storage" not in main_cpp
    assert "USE_ESP32_RTC_PREFERENCES" not in _define_names()


def test_restored_string_reserves_esp8266_store(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("globals_esp8266_storage_test.yaml"))
    # 15 characters plus the length byte, in RTC memory
    assert CORE.data[KEY_ESP8266][KEY_PREFERENCES] == [(preference_words(16), False)]
