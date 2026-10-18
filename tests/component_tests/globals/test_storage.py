"""Tests for the globals storage option."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components import globals as globals_component
from esphome.components.esp32 import KEY_VARIANT, VARIANT_ESP32, VARIANT_ESP32C2
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


def test_rtc_rejected_on_esp32_c2(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32C2}
    )
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


def test_omitted_storage_keeps_platform_default(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("globals_test.yaml"))
    assert (
        "new(my_restoring_int) globals::RestoringGlobalsComponent<int>(8)" in main_cpp
    )
    assert "globals::Storage" not in main_cpp
    assert "USE_ESP32_RTC_PREFERENCES" not in _define_names()
