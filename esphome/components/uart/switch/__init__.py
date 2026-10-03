import esphome.codegen as cg
from esphome.components import switch, uart
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_SEND_EVERY

from .. import payload_table, uart_ns, validate_raw_payload

DEPENDENCIES = ["uart"]

UARTSwitch = uart_ns.class_("UARTSwitch", switch.Switch, uart.UARTDevice, cg.Component)

CONF_TURN_OFF = "turn_off"
CONF_TURN_ON = "turn_on"

CONFIG_SCHEMA = (
    switch.switch_schema(UARTSwitch, block_inverted=True)
    .extend(
        {
            cv.Required(CONF_DATA): cv.Any(
                validate_raw_payload,
                cv.Schema(
                    {
                        cv.Optional(CONF_TURN_OFF): validate_raw_payload,
                        cv.Optional(CONF_TURN_ON): validate_raw_payload,
                    }
                ),
            ),
            cv.Optional(CONF_SEND_EVERY): cv.positive_time_period_milliseconds,
        },
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    data = config[CONF_DATA]
    if isinstance(data, dict):
        if data_on := data.get(CONF_TURN_ON):
            cg.add(var.set_data_on(payload_table(data_on), len(data_on)))
        if data_off := data.get(CONF_TURN_OFF):
            cg.add(var.set_data_off(payload_table(data_off), len(data_off)))
    else:
        cg.add(var.set_data_on(payload_table(data), len(data)))
        cg.add(var.set_single_state(True))
    if CONF_SEND_EVERY in config:
        cg.add(var.set_send_every(config[CONF_SEND_EVERY]))
