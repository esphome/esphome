from esphome.components import switch
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_SWITCHES
CustomSwitchConstructor = switch.switch_ns.class_('CustomSwitchConstructor')

PLATFORM_SCHEMA = switch.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(CustomSwitchConstructor),
    cv.Required(CONF_LAMBDA): cv.lambda_,
    cv.Required(CONF_SWITCHES):
        cv.ensure_list(switch.SWITCH_SCHEMA.extend({
            cv.GenerateID(): cv.declare_variable_id(switch.Switch),
        })),
})


def to_code(config):
    template_ = yield process_lambda(config[CONF_LAMBDA], [],
                                     return_type=std_vector.template(switch.SwitchPtr))

    rhs = CustomSwitchConstructor(template_)
    custom = variable(config[CONF_ID], rhs)
    for i, conf in enumerate(config[CONF_SWITCHES]):
        rhs = custom.Pget_switch(i)
        cg.add(rhs.set_name(conf[CONF_NAME]))
        switch.register_switch(rhs, conf)


BUILD_FLAGS = '-DUSE_CUSTOM_SWITCH'
