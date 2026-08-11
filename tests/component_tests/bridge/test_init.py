"""Tests for the bridge usb_uart platform's final validation."""

import pytest

from esphome import config_validation as cv
from esphome.const import CONF_UART_ID, PlatformFramework
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

CONF_USB_CDC_ACM_ID = "usb_cdc_acm_id"


def _set_esp32_s3(set_core_config: SetCoreConfigCallable, **kwargs) -> None:
    from esphome.components.esp32 import KEY_VARIANT, VARIANT_ESP32S3

    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: VARIANT_ESP32S3},
        **kwargs,
    )


def _final_validate(config: ConfigType) -> ConfigType:
    # usb_uart's schema reads the target platform and variant from CORE at import
    # time, so the platform module can only be imported after _set_esp32_s3() has run.
    from esphome.components.usb_uart import bridge

    return bridge._final_validate(config)


def _bridge_config(uart_id: str, cdc_id: str) -> dict:
    return {CONF_UART_ID: ID(uart_id), CONF_USB_CDC_ACM_ID: ID(cdc_id)}


def test_accepts_distinct_uart_and_cdc_interfaces(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_esp32_s3(set_core_config)
    _final_validate(_bridge_config("uart_0", "cdc_acm_1"))
    _final_validate(_bridge_config("uart_1", "cdc_acm_2"))


def test_rejects_two_bridges_sharing_a_uart(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_esp32_s3(set_core_config)
    _final_validate(_bridge_config("uart_0", "cdc_acm_1"))
    with pytest.raises(cv.Invalid, match="already bridged"):
        _final_validate(_bridge_config("uart_0", "cdc_acm_2"))


def test_rejects_two_bridges_sharing_a_cdc_interface(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_esp32_s3(set_core_config)
    _final_validate(_bridge_config("uart_0", "cdc_acm_1"))
    with pytest.raises(cv.Invalid, match="already bridged"):
        _final_validate(_bridge_config("uart_1", "cdc_acm_1"))


def test_rejects_uart_shared_with_another_component(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_esp32_s3(
        set_core_config,
        full_config={
            "sensor": [{"platform": "pzemac", CONF_UART_ID: ID("uart_0")}],
        },
    )
    with pytest.raises(cv.Invalid, match="exclusive"):
        _final_validate(_bridge_config("uart_0", "cdc_acm_1"))


def test_rejects_cdc_interface_shared_with_another_component(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # The CDC instance is itself a uart::UARTComponent, so other components can bind
    # it as a plain UART via uart_id -- that must be rejected just like UART sharing.
    _set_esp32_s3(
        set_core_config,
        full_config={
            "sensor": [{"platform": "pzemac", CONF_UART_ID: ID("cdc_acm_1")}],
        },
    )
    with pytest.raises(cv.Invalid, match="exclusive"):
        _final_validate(_bridge_config("uart_0", "cdc_acm_1"))


def test_rejects_uart_referenced_from_nested_config(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # References can sit arbitrarily deep, e.g. inside an automation's action list.
    _set_esp32_s3(
        set_core_config,
        full_config={
            "binary_sensor": [
                {
                    "platform": "gpio",
                    "on_press": [{"then": [{CONF_UART_ID: ID("uart_0")}]}],
                }
            ],
        },
    )
    with pytest.raises(cv.Invalid, match="exclusive"):
        _final_validate(_bridge_config("uart_0", "cdc_acm_1"))


def test_ignores_other_components_on_other_uarts(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_esp32_s3(
        set_core_config,
        full_config={
            "sensor": [{"platform": "pzemac", CONF_UART_ID: ID("uart_1")}],
            # The bridge domain itself is skipped: this bridge's own entry (and any
            # bridge-vs-bridge sharing, which the seen-set already rejects) must not
            # trip the exclusivity scan.
            "bridge": [_bridge_config("uart_0", "cdc_acm_1")],
        },
    )
    _final_validate(_bridge_config("uart_0", "cdc_acm_1"))
