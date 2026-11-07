"""Tests for the opentherm42 component's RMT/ISR datalink backend resolution."""

import pytest

from esphome import config_validation as cv
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def test_esp32_defaults_to_rmt(set_core_config: SetCoreConfigCallable) -> None:
    from esphome.components.esp32 import KEY_VARIANT, VARIANT_ESP32
    from esphome.components.opentherm42 import DATALINK_ESP32_RMT, _resolve_datalink

    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32}
    )

    assert _resolve_datalink({}) == DATALINK_ESP32_RMT


def test_explicit_isr_on_esp32_is_honored(
    set_core_config: SetCoreConfigCallable,
) -> None:
    from esphome.components.esp32 import KEY_VARIANT, VARIANT_ESP32
    from esphome.components.opentherm42 import (
        CONF_DATALINK,
        DATALINK_ISR,
        _resolve_datalink,
        _validate_datalink,
    )

    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32}
    )

    config = {CONF_DATALINK: DATALINK_ISR}
    _validate_datalink(config)  # should not raise
    assert _resolve_datalink(config) == DATALINK_ISR


def test_variant_without_rmt_hardware_defaults_to_isr(
    set_core_config: SetCoreConfigCallable,
) -> None:
    from esphome.components.esp32 import KEY_VARIANT, VARIANT_ESP32C2
    from esphome.components.opentherm42 import DATALINK_ISR, _resolve_datalink

    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32C2}
    )

    # Left unset, this silently resolves to ISR -- no error, since the user didn't ask for RMT.
    assert _resolve_datalink({}) == DATALINK_ISR


def test_explicit_rmt_on_variant_without_rmt_hardware_raises(
    set_core_config: SetCoreConfigCallable,
) -> None:
    from esphome.components.esp32 import KEY_VARIANT, VARIANT_ESP32C2
    from esphome.components.opentherm42 import (
        CONF_DATALINK,
        DATALINK_ESP32_RMT,
        _validate_datalink,
    )

    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32C2}
    )

    with pytest.raises(cv.Invalid, match="not available on"):
        _validate_datalink({CONF_DATALINK: DATALINK_ESP32_RMT})


def test_esp8266_always_uses_isr(set_core_config: SetCoreConfigCallable) -> None:
    from esphome.components.opentherm42 import DATALINK_ISR, _resolve_datalink

    set_core_config(PlatformFramework.ESP8266_ARDUINO)

    assert _resolve_datalink({}) == DATALINK_ISR


def test_explicit_rmt_on_esp8266_raises(set_core_config: SetCoreConfigCallable) -> None:
    from esphome.components.opentherm42 import (
        CONF_DATALINK,
        DATALINK_ESP32_RMT,
        _validate_datalink,
    )

    set_core_config(PlatformFramework.ESP8266_ARDUINO)

    with pytest.raises(cv.Invalid, match="not available on esp8266"):
        _validate_datalink({CONF_DATALINK: DATALINK_ESP32_RMT})
