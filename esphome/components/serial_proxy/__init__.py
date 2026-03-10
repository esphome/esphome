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
from esphome.const import CONF_ID, CONF_NAME
from esphome.core import CORE, coroutine_with_priority
from esphome.coroutine import CoroPriority

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["api", "uart"]

MULTI_CONF = True

serial_proxy_ns = cg.esphome_ns.namespace("serial_proxy")
SerialProxy = serial_proxy_ns.class_("SerialProxy", cg.Component, uart.UARTDevice)

api_enums_ns = cg.esphome_ns.namespace("api").namespace("enums")
SerialProxyPortType = api_enums_ns.enum("SerialProxyPortType")
SERIAL_PROXY_PORT_TYPES = {
    "TTL": SerialProxyPortType.SERIAL_PROXY_PORT_TYPE_TTL,
    "RS232": SerialProxyPortType.SERIAL_PROXY_PORT_TYPE_RS232,
    "RS485": SerialProxyPortType.SERIAL_PROXY_PORT_TYPE_RS485,
}

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
            cv.Required(CONF_PORT_TYPE): cv.enum(SERIAL_PROXY_PORT_TYPES, upper=True),
            cv.Optional(CONF_RTS_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_DTR_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


@coroutine_with_priority(CoroPriority.FINAL)
async def _add_serial_proxy_count_define():
    """Emit the SERIAL_PROXY_COUNT define once with the final instance count."""
    count = _get_data().count
    if count > 0:
        cg.add_define("SERIAL_PROXY_COUNT", count)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(cg.App.register_serial_proxy(var))
    cg.add(var.set_name(config[CONF_NAME]))
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
