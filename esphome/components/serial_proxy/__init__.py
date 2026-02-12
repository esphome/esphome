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

from esphome import pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["api", "uart"]

MULTI_CONF = True

serial_proxy_ns = cg.esphome_ns.namespace("serial_proxy")
SerialProxy = serial_proxy_ns.class_("SerialProxy", cg.Component, uart.UARTDevice)

CONF_RTS_PIN = "rts_pin"
CONF_DTR_PIN = "dtr_pin"


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SerialProxy),
            cv.Optional(CONF_RTS_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_DTR_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(cg.App.register_serial_proxy(var))
    cg.add_define("USE_SERIAL_PROXY")

    if CONF_RTS_PIN in config:
        rts_pin = await cg.gpio_pin_expression(config[CONF_RTS_PIN])
        cg.add(var.set_rts_pin(rts_pin))

    if CONF_DTR_PIN in config:
        dtr_pin = await cg.gpio_pin_expression(config[CONF_DTR_PIN])
        cg.add(var.set_dtr_pin(dtr_pin))

    # Request UART to wake the main loop when data arrives for low-latency processing
    uart.request_wake_loop_on_rx()
