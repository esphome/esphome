from esphome.components import switch
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_INVERTED, CONF_NAME, CONF_ICON, ICON_POWER

shutdown_ns = cg.esphome_ns.namespace('shutdown')
ShutdownSwitch = shutdown_ns.class_('ShutdownSwitch', switch.Switch, cg.Component)

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(ShutdownSwitch),

    cv.Optional(CONF_INVERTED): cv.invalid("Shutdown switches do not support inverted mode!"),

    cv.Optional(CONF_ICON, default=ICON_POWER): switch.icon
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME])
    yield cg.register_component(var, config)
    yield switch.register_switch(var, config)
