import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, uart
from esphome.const import CONF_DATA, CONF_SEND_EVERY
from esphome.core import HexInt
from .. import uart_ns, validate_raw_data

DEPENDENCIES = ["uart"]

UARTSwitch = uart_ns.class_("UARTSwitch", switch.Switch, uart.UARTDevice, cg.Component)


CONFIG_SCHEMA = (
    switch.switch_schema(UARTSwitch, block_inverted=True)
    .extend(
        {
            cv.Required(CONF_DATA): validate_raw_data,
            cv.Optional(CONF_SEND_EVERY): cv.positive_time_period_milliseconds,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    data = config[CONF_DATA]
    if isinstance(data, bytes):
        data = [HexInt(x) for x in data]
    cg.add(var.set_data(data))

    if CONF_SEND_EVERY in config:
        cg.add(var.set_send_every(config[CONF_SEND_EVERY]))
