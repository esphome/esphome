import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, uart
from esphome.const import CONF_DATA, CONF_ID, CONF_INVERTED
from esphome.core import HexInt
from .. import uart_ns, validate_raw_data

DEPENDENCIES = ['uart']

UARTSwitch = uart_ns.class_('UARTSwitch', switch.Switch, uart.UARTDevice, cg.Component)


CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTSwitch),
    cv.Required(CONF_DATA): validate_raw_data,
    cv.Optional(CONF_INVERTED): cv.invalid("UART switches do not support inverted mode!"),
}).extend(uart.UART_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield switch.register_switch(var, config)
    yield uart.register_uart_device(var, config)

    data = config[CONF_DATA]
    if isinstance(data, bytes):
        data = [HexInt(x) for x in data]
    cg.add(var.set_data(data))
