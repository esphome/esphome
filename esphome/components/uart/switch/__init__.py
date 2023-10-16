import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, uart
from esphome.const import CONF_DATA, CONF_SEND_EVERY
from esphome.core import HexInt
from .. import uart_ns, validate_raw_data

DEPENDENCIES = ["uart"]

UARTSwitch = uart_ns.class_("UARTSwitch", switch.Switch, uart.UARTDevice, cg.Component)

CONF_DATA_ON = "data_on"
CONF_DATA_OFF = "data_off"


def validate_data(config):
    if (CONF_DATA_ON in config or CONF_DATA_OFF in config) and (
        CONF_DATA in config or CONF_SEND_EVERY in config
    ):
        raise cv.Invalid(
            '"data" or "send_every" can\'t be used with "data_on" or "data_off"'
        )
    return config


CONFIG_SCHEMA = cv.All(
    switch.switch_schema(UARTSwitch, block_inverted=True)
    .extend(
        {
            cv.Optional(CONF_DATA): validate_raw_data,
            cv.Optional(CONF_DATA_OFF): validate_raw_data,
            cv.Optional(CONF_DATA_ON): validate_raw_data,
            cv.Optional(CONF_SEND_EVERY): cv.positive_time_period_milliseconds,
        },
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.has_at_least_one_key(CONF_DATA, CONF_DATA_ON, CONF_DATA_OFF),
    validate_data,
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if data := config.get(CONF_DATA):
        if isinstance(data, bytes):
            data = [HexInt(x) for x in data]
        cg.add(var.set_data(data))

    if data := config.get(CONF_DATA_ON):
        if isinstance(data, bytes):
            data = [HexInt(x) for x in data]
        cg.add(var.set_data_on(data))

    if data := config.get(CONF_DATA_OFF):
        if isinstance(data, bytes):
            data = [HexInt(x) for x in data]
        cg.add(var.set_data_off(data))

    if CONF_SEND_EVERY in config:
        cg.add(var.set_send_every(config[CONF_SEND_EVERY]))
