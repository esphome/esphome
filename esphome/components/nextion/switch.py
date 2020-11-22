import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, uart
from esphome.const import CONF_COMPONENT_ID, CONF_PAGE_ID, CONF_ID
from . import nextion_ns, CONF_NEXTION_ID
from .display import Nextion

DEPENDENCIES = ['display']

CONF_BUTTON_ID = 'button_id'

NextionSwitch = nextion_ns.class_('NextionSwitch', switch.Switch, cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(NextionSwitch),
    cv.GenerateID(CONF_NEXTION_ID): cv.use_id(Nextion),
    cv.Required(CONF_BUTTON_ID): cv.string,
    cv.Required(CONF_PAGE_ID): cv.uint8_t,
    cv.Required(CONF_COMPONENT_ID): cv.uint8_t,
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield switch.register_switch(var, config)
    yield uart.register_uart_device(var, config)

    hub = yield cg.get_variable(config[CONF_NEXTION_ID])
    cg.add(hub.register_switch_component(var))

    cg.add(var.set_component_id(config[CONF_COMPONENT_ID]))
    cg.add(var.set_page_id(config[CONF_PAGE_ID]))
    cg.add(var.set_device_id(config[CONF_BUTTON_ID]))
