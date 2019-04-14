from esphome.components import output, switch
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_NAME, CONF_OUTPUT
from .. import output_ns

OutputSwitch = output_ns.class_('OutputSwitch', switch.Switch, cg.Component)

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(OutputSwitch),
    cv.Required(CONF_OUTPUT): cv.use_variable_id(output.BinaryOutput),
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    output_ = yield cg.get_variable(config[CONF_OUTPUT])
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], output_)
    yield cg.register_component(var, config)
    yield switch.register_switch(var, config)
