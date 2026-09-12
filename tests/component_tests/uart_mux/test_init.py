"""Tests for the uart_mux component's final validation and its bridge exemption."""

import pytest

from esphome import config_validation as cv
from esphome.config import Config
from esphome.const import CONF_ID, CONF_UART_ID, PlatformFramework
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

CONF_BRIDGE_ID = "bridge_id"
CONF_USB_CDC_ACM_ID = "usb_cdc_acm_id"


def _set_esp32_s3(set_core_config: SetCoreConfigCallable, **kwargs) -> None:
    from esphome.components.esp32 import KEY_VARIANT, VARIANT_ESP32S3

    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: VARIANT_ESP32S3},
        **kwargs,
    )


def _bridge_conf(bridge_id: str, uart_id: str) -> ConfigType:
    return {
        CONF_ID: ID(bridge_id),
        CONF_UART_ID: ID(uart_id),
        CONF_USB_CDC_ACM_ID: ID("cdc_acm_1"),
    }


def _full_config(**domains) -> Config:
    """A full config declaring uart_0, as the ID pass leaves it, so the bridge's debug
    check can resolve its uart_id."""
    full = Config()
    full["uart"] = [{CONF_ID: ID("uart_0")}]
    full.declare_ids.append((full["uart"][0][CONF_ID], ["uart", 0, CONF_ID]))
    full.update(domains)
    return full


def _mux_conf(bridge_id: str, uart_id: str) -> ConfigType:
    return {
        CONF_ID: ID("mux_0"),
        CONF_BRIDGE_ID: ID(bridge_id),
        CONF_UART_ID: ID(uart_id),
    }


def test_mux_accepts_the_bridged_uart(set_core_config: SetCoreConfigCallable) -> None:
    _set_esp32_s3(
        set_core_config,
        full_config={"bridge": [_bridge_conf("bridge_0", "uart_0")]},
    )
    from esphome.components import uart_mux

    uart_mux._final_validate(_mux_conf("bridge_0", "uart_0"))


def test_mux_rejects_a_different_uart(set_core_config: SetCoreConfigCallable) -> None:
    _set_esp32_s3(
        set_core_config,
        full_config={"bridge": [_bridge_conf("bridge_0", "uart_0")]},
    )
    from esphome.components import uart_mux

    with pytest.raises(cv.Invalid, match="must be the UART bridged by 'bridge_0'"):
        uart_mux._final_validate(_mux_conf("bridge_0", "uart_1"))


def test_bridge_allows_its_own_mux_on_its_uart(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_esp32_s3(
        set_core_config,
        full_config=_full_config(uart_mux=[_mux_conf("bridge_0", "uart_0")]),
    )
    from esphome.components.cdc_acm_uart import bridge

    bridge._final_validate(_bridge_conf("bridge_0", "uart_0"))


def test_bridge_rejects_a_mux_bound_to_another_bridge(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_esp32_s3(
        set_core_config,
        full_config=_full_config(uart_mux=[_mux_conf("bridge_1", "uart_0")]),
    )
    from esphome.components.cdc_acm_uart import bridge

    with pytest.raises(cv.Invalid, match="also used by 'uart_mux'"):
        bridge._final_validate(_bridge_conf("bridge_0", "uart_0"))
