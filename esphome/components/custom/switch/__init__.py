from esphome.components import switch
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_SWITCHES
from .. import custom_ns


CustomSwitchConstructor = custom_ns.class_('CustomSwitchConstructor')

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(CustomSwitchConstructor),
    cv.Required(CONF_LAMBDA): cv.lambda_,
    cv.Required(CONF_SWITCHES):
        cv.ensure_list(switch.SWITCH_SCHEMA.extend({
            cv.GenerateID(): cv.declare_id(switch.Switch),
        })),
})


def to_code(config):
    template_ = yield cg.process_lambda(
        config[CONF_LAMBDA], [], return_type=cg.std_vector.template(switch.SwitchPtr))

    rhs = CustomSwitchConstructor(template_)
    var = cg.variable(config[CONF_ID], rhs)
    for i, conf in enumerate(config[CONF_SWITCHES]):
        switch_ = cg.new_Pvariable(conf[CONF_ID], var.get_switch(i))
        cg.add(switch_.set_name(conf[CONF_NAME]))
        yield switch.register_switch(switch_, conf)
