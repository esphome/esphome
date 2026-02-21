import esphome.codegen as cg
from esphome.components import esp32, uart
from esphome.components.esp32 import (
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    add_idf_sdkconfig_option,
)
from esphome.components.uart import debug_to_code, maybe_empty_debug
from esphome.components.zephyr import zephyr_add_cdc_acm, zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEBUG,
    CONF_DISABLED,
    CONF_ID,
    CONF_RX_BUFFER_SIZE,
    CONF_TX_BUFFER_SIZE,
)
from esphome.core import CORE
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
AUTO_LOAD = ["uart"]

# needs https://github.com/esphome/esphome/pull/14174
# def DEPENDENCIES():
#     if CORE.using_zephyr:
#         return []
#     return ["tinyusb"]


CONF_INTERFACES = "interfaces"

usb_cdc_acm_ns = cg.esphome_ns.namespace("usb_cdc_acm")
USBCDCACMComponent = usb_cdc_acm_ns.class_("USBCDCACMComponent", cg.Component)
USBCDCACMInstance = usb_cdc_acm_ns.class_(
    "USBCDCACMInstance", uart.UARTComponent, cg.Parented.template(USBCDCACMComponent)
)


# Schema for individual CDC ACM interface instances
INTERFACE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(USBCDCACMInstance),
        cv.Optional(CONF_DEBUG): maybe_empty_debug,
        cv.Optional(CONF_DISABLED, default=False): cv.boolean,
    }
)

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
    # needs https://github.com/esphome/esphome/pull/14174
    # esp32.only_on_variant(
    #     supported=[VARIANT_ESP32P4, VARIANT_ESP32S2, VARIANT_ESP32S3],
    # ),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    num_interfaces = len(config[CONF_INTERFACES])
    cg.add_define("ESPHOME_MAX_USB_CDC_INSTANCES", num_interfaces)
    all_port_disabled = True
    # Create and register interface instances
    for interface_index, interface_conf in enumerate(config[CONF_INTERFACES]):
        if interface_conf[CONF_DISABLED]:
            continue
        all_port_disabled = False
        interface = None
        if CORE.using_zephyr:
            port = f"cdc_acm_uart{interface_index}"
            zephyr_add_cdc_acm(config, interface_index)
            interface = cg.new_Pvariable(
                interface_conf[CONF_ID],
                MockObj(f"DEVICE_DT_GET_OR_NULL(DT_NODELABEL({port}))"),
            )
        else:
            interface = cg.new_Pvariable(interface_conf[CONF_ID])
        await cg.register_parented(interface, var)
        cg.add(interface.set_interface_number(interface_index))
        cg.add(var.add_interface(interface))
        if CONF_DEBUG in interface_conf:
            await debug_to_code(interface_conf[CONF_DEBUG], interface)
    if CORE.using_zephyr:
        if not all_port_disabled:
            zephyr_add_prj_conf("UART_LINE_CTRL", True)
            zephyr_add_prj_conf("CDC_ACM_DTE_RATE_CALLBACK_SUPPORT", True)
            cg.add_define(
                "ESPHOME_CDC_RX_RING_BUFFER_SIZE", config[CONF_RX_BUFFER_SIZE]
            )
            cg.add_define(
                "ESPHOME_CDC_TX_RING_BUFFER_SIZE", config[CONF_TX_BUFFER_SIZE]
            )
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
