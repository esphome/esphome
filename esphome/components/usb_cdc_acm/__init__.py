from collections.abc import Iterator

import esphome.codegen as cg
from esphome.components import esp32, uart
from esphome.components.esp32 import (
    VARIANT_ESP32H4,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
    add_idf_sdkconfig_option,
)
from esphome.components.uart import debug_to_code, maybe_empty_debug, uart_ns
from esphome.components.zephyr import (
    zephyr_add_prj_conf,
    zephyr_cdc_acm_label,
    zephyr_claim_cdc_acm,
    zephyr_variant_family,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEBUG,
    CONF_DISABLED,
    CONF_ID,
    CONF_RX_BUFFER_SIZE,
    CONF_TX_BUFFER_SIZE,
    PLATFORM_NRF52,
    PLATFORM_ZEPHYR,
)
from esphome.core import CORE
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
DOMAIN = "usb_cdc_acm"


def AUTO_LOAD() -> list[str]:
    if CORE.using_zephyr:
        return ["uart"]
    return ["uart", "tinyusb"]


CONF_INTERFACES = "interfaces"

usb_cdc_acm_ns = cg.esphome_ns.namespace("usb_cdc_acm")
USBCDCACMComponent = usb_cdc_acm_ns.class_("USBCDCACMComponent", cg.Component)
USBCDCACMInstance = usb_cdc_acm_ns.class_(
    "USBCDCACMInstance", uart.UARTComponent, cg.Parented.template(USBCDCACMComponent)
)


# Schema for individual CDC ACM interface instances
INTERFACE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.All(cv.declare_id(USBCDCACMInstance), uart.mark_unclocked),
        cv.Optional(CONF_DEBUG): maybe_empty_debug,
        cv.Optional(CONF_DISABLED): cv.All(
            cv.boolean, cv.only_on([PLATFORM_NRF52, PLATFORM_ZEPHYR])
        ),
    }
)


def _enabled_interfaces(config: ConfigType) -> Iterator[tuple[int, ConfigType]]:
    for index, conf in enumerate(config[CONF_INTERFACES]):
        if not conf.get(CONF_DISABLED, False):
            yield index, conf


# platform: zephyr families with a USB device controller, as the logger's USB_CDC option
ZEPHYR_USB_FAMILIES = {"nordic", "rpi_pico", "renesas", "stm32", "nxp"}


def _only_on_zephyr_usb(config: ConfigType) -> ConfigType:
    if not CORE.is_zephyr or zephyr_variant_family() not in ZEPHYR_USB_FAMILIES:
        raise cv.Invalid(
            "This feature is only available on platform: zephyr variants with USB"
        )
    return config


def _validate_any_interface_enabled(config: ConfigType) -> ConfigType:
    if next(_enabled_interfaces(config), None) is None:
        raise cv.Invalid("At least one interface must be enabled")
    return config


# Main component schema
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(USBCDCACMComponent),
            cv.Optional(CONF_RX_BUFFER_SIZE, default=256): cv.All(
                cv.validate_bytes, cv.uint16_t
            ),
            cv.Optional(CONF_TX_BUFFER_SIZE, default=256): cv.All(
                cv.validate_bytes, cv.uint16_t
            ),
            cv.Optional(CONF_INTERFACES, default=[{}]): cv.All(
                cv.ensure_list(INTERFACE_SCHEMA),
                cv.Length(min=1, max=2),  # At least 1, at most 2 interfaces
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_any_interface_enabled,
    cv.Any(
        cv.only_on_nrf52,
        _only_on_zephyr_usb,
        cv.All(
            cv.only_on_esp32,
            esp32.only_on_variant(
                supported=[
                    VARIANT_ESP32H4,
                    VARIANT_ESP32P4,
                    VARIANT_ESP32S2,
                    VARIANT_ESP32S3,
                    VARIANT_ESP32S31,
                ],
            ),
        ),
    ),
)


def _final_validate(config: ConfigType) -> None:
    if not CORE.using_zephyr:
        return
    for index, _ in _enabled_interfaces(config):
        zephyr_claim_cdc_acm(index, DOMAIN)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    num_interfaces = len(config[CONF_INTERFACES])
    cg.add_define("ESPHOME_MAX_USB_CDC_INSTANCES", num_interfaces)
    # Create and register interface instances
    for index, conf in _enabled_interfaces(config):
        if CORE.is_nrf52:
            args = (
                MockObj(f"DEVICE_DT_GET_OR_NULL(DT_NODELABEL(cdc_acm_uart{index}))"),
            )
        elif CORE.is_zephyr:
            label = zephyr_cdc_acm_label(index)
            args = (
                MockObj(f"DEVICE_DT_GET_OR_NULL(DT_NODELABEL({label}))"),
                MockObj(f"DT_PROP(DT_NODELABEL({label}), tx_fifo_size)"),
            )
        else:
            args = ()
        interface = cg.new_Pvariable(conf[CONF_ID], *args)
        await cg.register_parented(interface, var)
        cg.add(interface.set_interface_number(index))
        cg.add(var.add_interface(interface))
        if (debug := conf.get(CONF_DEBUG)) is not None:
            cg.add_global(uart_ns.using)
            await debug_to_code(debug, interface)
    if CORE.using_zephyr:
        zephyr_add_prj_conf("UART_LINE_CTRL", True)
        # Zephyr 4.5 keeps the ring_buf claim/finish API only with this option
        zephyr_add_prj_conf("RING_BUFFER", True)
        if CORE.is_nrf52:
            zephyr_add_prj_conf("CDC_ACM_DTE_RATE_CALLBACK_SUPPORT", True)
            # Every CDC ACM instance (the logger's and any the board tree adds) holds a transfer
            # slot per endpoint; the default of 4 leaves a third instance without any
            zephyr_add_prj_conf("USB_MAX_NUM_TRANSFERS", 8)
        cg.add_define("ESPHOME_CDC_RX_RING_BUFFER_SIZE", config[CONF_RX_BUFFER_SIZE])
        cg.add_define("ESPHOME_CDC_TX_RING_BUFFER_SIZE", config[CONF_TX_BUFFER_SIZE])
    else:
        # Configure TinyUSB with the correct number of CDC interfaces
        add_idf_sdkconfig_option("CONFIG_TINYUSB_CDC_ENABLED", True)
        add_idf_sdkconfig_option("CONFIG_TINYUSB_CDC_COUNT", num_interfaces)
        add_idf_sdkconfig_option(
            "CONFIG_TINYUSB_CDC_RX_BUFSIZE", config[CONF_RX_BUFFER_SIZE]
        )
        add_idf_sdkconfig_option(
            "CONFIG_TINYUSB_CDC_TX_BUFSIZE", config[CONF_TX_BUFFER_SIZE]
        )
