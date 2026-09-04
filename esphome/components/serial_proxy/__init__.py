"""
Serial Proxy component for ESPHome.

WARNING: This component is EXPERIMENTAL. The API (both Python configuration
and C++ interfaces) may change at any time without following the normal
breaking changes policy. Use at your own risk.

Once the API is considered stable, this warning will be removed.

Provides a proxy to/from a serial interface on the ESPHome device, allowing
Home Assistant to connect to the serial port and send/receive data to/from
an arbitrary serial device.
"""

from dataclasses import dataclass

from esphome import pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_UART_ID
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["api", "uart"]

MULTI_CONF = True

serial_proxy_ns = cg.esphome_ns.namespace("serial_proxy")
SerialProxy = serial_proxy_ns.class_("SerialProxy", cg.Component, uart.UARTDevice)
SerialProxyTap = serial_proxy_ns.class_("SerialProxyTap")

api_enums_ns = cg.esphome_ns.namespace("api").namespace("enums")
SerialProxyPortType = api_enums_ns.enum("SerialProxyPortType")
# User-selectable electrical types. USB_SERIAL is deliberately absent: it is derived
# from the uart_id pointing at a usb_uart channel, never set by the user.
SERIAL_PROXY_PORT_TYPES = {
    "TTL": SerialProxyPortType.SERIAL_PROXY_PORT_TYPE_TTL,
    "RS232": SerialProxyPortType.SERIAL_PROXY_PORT_TYPE_RS232,
    "RS485": SerialProxyPortType.SERIAL_PROXY_PORT_TYPE_RS485,
}
PORT_TYPE_USB_SERIAL = SerialProxyPortType.SERIAL_PROXY_PORT_TYPE_USB_SERIAL

CONF_DTR_PIN = "dtr_pin"
CONF_PORT_TYPE = "port_type"
CONF_RTS_PIN = "rts_pin"

DOMAIN = "serial_proxy"


@dataclass
class SerialProxyData:
    count: int = 0


def _get_data() -> SerialProxyData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = SerialProxyData()
    return CORE.data[DOMAIN]


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SerialProxy),
            cv.Required(CONF_NAME): cv.string_strict,
            cv.Optional(CONF_PORT_TYPE): cv.enum(SERIAL_PROXY_PORT_TYPES, upper=True),
            cv.Optional(CONF_RTS_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_DTR_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


def _uses_usb_uart(config: ConfigType, full_config: ConfigType) -> bool:
    from esphome.components.usb_uart import is_usb_uart_channel

    return is_usb_uart_channel(config[CONF_UART_ID], full_config)


def _final_validate(config: ConfigType) -> ConfigType:
    if _uses_usb_uart(config, fv.full_config.get()):
        if CONF_PORT_TYPE in config:
            raise cv.Invalid(
                f"{CONF_PORT_TYPE} is set automatically for USB serial ports"
            )
    elif CONF_PORT_TYPE not in config:
        raise cv.Invalid(f"{CONF_PORT_TYPE} is required")
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


@coroutine_with_priority(CoroPriority.FINAL)
async def _add_serial_proxy_count_define() -> None:
    """Emit the SERIAL_PROXY_COUNT define once with the final instance count."""
    count = _get_data().count
    if count > 0:
        cg.add_define("SERIAL_PROXY_COUNT", count)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(cg.App.register_serial_proxy(var))
    cg.add(var.set_name(config[CONF_NAME]))
    if _uses_usb_uart(config, CORE.config):
        cg.add(var.set_port_type(PORT_TYPE_USB_SERIAL))
        channel = await cg.get_variable(config[CONF_UART_ID])
        cg.add(var.set_usb_channel(channel))
        cg.add_define("USE_SERIAL_PROXY_USB_INFO")
    else:
        cg.add(var.set_port_type(config[CONF_PORT_TYPE]))
    cg.add_define("USE_SERIAL_PROXY")

    # Track instance count for the FINAL priority define
    data = _get_data()
    if data.count == 0:
        # Schedule the count define job only once (on the first instance)
        CORE.add_job(_add_serial_proxy_count_define)
    data.count += 1

    if CONF_RTS_PIN in config:
        rts_pin = await cg.gpio_pin_expression(config[CONF_RTS_PIN])
        cg.add(var.set_rts_pin(rts_pin))

    if CONF_DTR_PIN in config:
        dtr_pin = await cg.gpio_pin_expression(config[CONF_DTR_PIN])
        cg.add(var.set_dtr_pin(dtr_pin))
