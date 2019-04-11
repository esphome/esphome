from esphome.components import switch
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_INVERTED, CONF_NAME

restart_ns = cg.esphome_ns.namespace('restart')
RestartSwitch = restart_ns.class_('RestartSwitch', switch.Switch, cg.Component)

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(RestartSwitch),
    cv.Optional(CONF_INVERTED): cv.invalid("Restart switches do not support inverted mode!"),
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    yield cg.register_component(var, config)
    yield switch.register_switch(var, config)
