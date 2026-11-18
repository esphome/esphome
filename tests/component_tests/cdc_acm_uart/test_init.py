"""Tests for the bridge cdc_acm_uart platform's final validation."""

import pytest

from esphome import config_validation as cv
from esphome.components import usb_cdc_acm
from esphome.components.cdc_acm_uart import bridge
from esphome.components.cdc_acm_uart.bridge import CONF_USB_CDC_ACM_ID
from esphome.config import Config
from esphome.const import (
    CONF_DEBUG,
    CONF_DUMMY_RECEIVER,
    CONF_ID,
    CONF_UART_ID,
    PlatformFramework,
)
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.helpers import uart_full_config
from tests.component_tests.types import SetCoreConfigCallable

_final_validate = bridge._final_validate


def _set_esp32_s3(
    set_core_config: SetCoreConfigCallable,
    full_config: Config,
    interfaces: list[ConfigType] | None = None,
) -> None:
    """Set up an ESP32-S3 that also declares cdc_acm_1 and cdc_acm_2 (or interfaces)."""
    from esphome.components.esp32 import KEY_VARIANT, VARIANT_ESP32S3

    interfaces = interfaces or [{CONF_ID: ID("cdc_acm_1")}, {CONF_ID: ID("cdc_acm_2")}]
    full_config[usb_cdc_acm.DOMAIN] = {usb_cdc_acm.CONF_INTERFACES: interfaces}
    for index, interface in enumerate(interfaces):
        path = [usb_cdc_acm.DOMAIN, usb_cdc_acm.CONF_INTERFACES, index, CONF_ID]
        full_config.declare_ids.append((interface[CONF_ID], path))
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: VARIANT_ESP32S3},
        full_config=full_config,
    )


def _bridge_config(uart_id: str, cdc_id: str) -> dict:
    return {CONF_UART_ID: ID(uart_id), CONF_USB_CDC_ACM_ID: ID(cdc_id)}


def test_accepts_distinct_uart_and_cdc_interfaces(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_esp32_s3(set_core_config, full_config=uart_full_config())
    _final_validate(_bridge_config("uart_0", "cdc_acm_1"))
    _final_validate(_bridge_config("uart_1", "cdc_acm_2"))


def test_rejects_two_bridges_sharing_a_uart(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_esp32_s3(set_core_config, full_config=uart_full_config())
    _final_validate(_bridge_config("uart_0", "cdc_acm_1"))
    with pytest.raises(cv.Invalid, match="already used by another 'bridge' entry"):
        _final_validate(_bridge_config("uart_0", "cdc_acm_2"))


def test_rejects_two_bridges_sharing_a_cdc_interface(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_esp32_s3(set_core_config, full_config=uart_full_config())
    _final_validate(_bridge_config("uart_0", "cdc_acm_1"))
    with pytest.raises(cv.Invalid, match="already used by another 'bridge' entry"):
        _final_validate(_bridge_config("uart_1", "cdc_acm_1"))


def test_rejects_uart_shared_with_another_component(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_esp32_s3(
        set_core_config,
        full_config=uart_full_config(
            sensor=[{"platform": "pzemac", CONF_UART_ID: ID("uart_0")}],
        ),
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
        full_config=uart_full_config(
            sensor=[{"platform": "pzemac", CONF_UART_ID: ID("cdc_acm_1")}],
        ),
    )
    with pytest.raises(cv.Invalid, match="exclusive"):
        _final_validate(_bridge_config("uart_0", "cdc_acm_1"))


def test_rejects_cdc_interface_shared_under_usb_cdc_acm_id(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # Naming the interface under usb_cdc_acm_id instead of uart_id is rejected too.
    _set_esp32_s3(
        set_core_config,
        full_config=uart_full_config(
            sensor=[{"platform": "pzemac", CONF_USB_CDC_ACM_ID: ID("cdc_acm_1")}],
        ),
    )
    with pytest.raises(
        cv.Invalid,
        match="The UART 'cdc_acm_1' is also used by 'sensor'. "
        "bridge requires exclusive use of that UART.",
    ):
        _final_validate(_bridge_config("uart_0", "cdc_acm_1"))


def test_rejects_uart_referenced_from_nested_config(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # References can sit arbitrarily deep, e.g. inside an automation's action list.
    _set_esp32_s3(
        set_core_config,
        full_config=uart_full_config(
            binary_sensor=[
                {
                    "platform": "gpio",
                    "on_press": [{"then": [{CONF_UART_ID: ID("uart_0")}]}],
                }
            ],
        ),
    )
    with pytest.raises(cv.Invalid, match="exclusive"):
        _final_validate(_bridge_config("uart_0", "cdc_acm_1"))


def test_ignores_other_components_on_other_uarts(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_esp32_s3(
        set_core_config,
        full_config=uart_full_config(
            sensor=[{"platform": "pzemac", CONF_UART_ID: ID("uart_1")}],
            # The bridge domain itself is skipped: this bridge's own entry (and any
            # bridge-vs-bridge sharing, which the claim already rejects) must not
            # trip the exclusivity scan.
            bridge=[_bridge_config("uart_0", "cdc_acm_1")],
        ),
    )
    _final_validate(_bridge_config("uart_0", "cdc_acm_1"))


def test_rejects_debug_on_bridged_uart(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # The bridge talks to the IDF driver directly, so the uart debugger would see
    # nothing and its dummy_receiver would steal RX bytes.
    _set_esp32_s3(
        set_core_config,
        full_config=uart_full_config(uarts=[{CONF_ID: ID("uart_0"), CONF_DEBUG: {}}]),
    )
    with pytest.raises(cv.Invalid, match="debug"):
        _final_validate(_bridge_config("uart_0", "cdc_acm_1"))


def test_allows_debug_on_other_uart(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _set_esp32_s3(
        set_core_config,
        full_config=uart_full_config(
            uarts=[{CONF_ID: ID("uart_0")}, {CONF_ID: ID("uart_1"), CONF_DEBUG: {}}]
        ),
    )
    _final_validate(_bridge_config("uart_0", "cdc_acm_1"))


def test_rejects_dummy_receiver_on_bridged_cdc_interface(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # The bridge drains the interface's RX ring buffer; a dummy_receiver steals from it.
    cdc_conf = {CONF_ID: ID("cdc_acm_1"), CONF_DEBUG: {CONF_DUMMY_RECEIVER: True}}
    _set_esp32_s3(
        set_core_config, full_config=uart_full_config(), interfaces=[cdc_conf]
    )
    with pytest.raises(cv.Invalid, match="drops the bytes bridge should forward"):
        _final_validate(_bridge_config("uart_0", "cdc_acm_1"))
