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
from esphome.components.const import CONF_MANUFACTURER
from esphome.components.usb_uart import is_usb_uart_channel
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_IDENTITY, CONF_NAME, CONF_UART_ID
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
SERIAL_PROXY_PORT_TYPES = {
    "TTL": SerialProxyPortType.SERIAL_PROXY_PORT_TYPE_TTL,
    "RS232": SerialProxyPortType.SERIAL_PROXY_PORT_TYPE_RS232,
    "RS485": SerialProxyPortType.SERIAL_PROXY_PORT_TYPE_RS485,
    "USB_SERIAL": SerialProxyPortType.SERIAL_PROXY_PORT_TYPE_USB_SERIAL,
}

CONF_DTR_PIN = "dtr_pin"
CONF_PORT_TYPE = "port_type"
CONF_PRODUCT = "product"
CONF_RTS_PIN = "rts_pin"
CONF_SERIAL_NUMBER = "serial_number"

DOMAIN = "serial_proxy"


@dataclass
class SerialProxyData:
    count: int = 0


def _get_data() -> SerialProxyData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = SerialProxyData()
    return CORE.data[DOMAIN]


# What the port claims to be, for a device that has no descriptors of its own to read.
# Each value may be a lambda, run once in setup(), for identifiers that differ per unit.
IDENTITY_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_MANUFACTURER): cv.templatable(cv.string),
            cv.Optional(CONF_PRODUCT): cv.templatable(cv.string),
            cv.Optional(CONF_SERIAL_NUMBER): cv.templatable(cv.string),
        }
    ),
    cv.has_at_least_one_key(CONF_MANUFACTURER, CONF_PRODUCT, CONF_SERIAL_NUMBER),
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SerialProxy),
            cv.Required(CONF_NAME): cv.string_strict,
            cv.Required(CONF_PORT_TYPE): cv.enum(SERIAL_PROXY_PORT_TYPES, upper=True),
            cv.Optional(CONF_RTS_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_DTR_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_IDENTITY): IDENTITY_SCHEMA,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


def _final_validate(config: ConfigType) -> ConfigType:
    is_usb = is_usb_uart_channel(config[CONF_UART_ID], fv.full_config.get())
    if config[CONF_PORT_TYPE] == "USB_SERIAL" and not is_usb:
        raise cv.Invalid(
            f"{CONF_PORT_TYPE} USB_SERIAL requires {CONF_UART_ID} to be a usb_uart channel"
        )
    if is_usb and CONF_IDENTITY in config:
        raise cv.Invalid(
            f"{CONF_IDENTITY} is read from the USB descriptors on a usb_uart channel; remove it"
        )
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
    cg.add(var.set_port_type(config[CONF_PORT_TYPE]))
    if is_usb_uart_channel(config[CONF_UART_ID], CORE.config):
        channel = await cg.get_variable(config[CONF_UART_ID])
        cg.add(var.set_usb_channel(channel))
        cg.add_define("USE_SERIAL_PROXY_USB_IDENTITY")
    if (identity := config.get(CONF_IDENTITY)) is not None:
        cg.add_define("USE_SERIAL_PROXY_CONFIGURED_IDENTITY")
        for key, setter in (
            (CONF_MANUFACTURER, var.set_identity_manufacturer),
            (CONF_PRODUCT, var.set_identity_product),
            (CONF_SERIAL_NUMBER, var.set_identity_serial_number),
        ):
            if (value := identity.get(key)) is not None:
                cg.add(setter(await cg.templatable(value, [], cg.std_string)))
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
