import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import automation
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_LAMBDA, CONF_MAKE_ID, CONF_NAME, CONF_TURN_OFF_ACTION, \
    CONF_TURN_ON_ACTION, CONF_OPTIMISTIC
from esphomeyaml.helpers import App, Application, process_lambda, variable, NoArg, add, bool_, \
    optional

MakeTemplateSwitch = Application.MakeTemplateSwitch

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeTemplateSwitch),
    vol.Optional(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_OPTIMISTIC): cv.boolean,
    vol.Optional(CONF_TURN_OFF_ACTION): automation.validate_automation(single=True),
    vol.Optional(CONF_TURN_ON_ACTION): automation.validate_automation(single=True),
}), cv.has_at_least_one_key(CONF_LAMBDA, CONF_OPTIMISTIC))


def to_code(config):
    rhs = App.make_template_switch(config[CONF_NAME])
    make = variable(config[CONF_MAKE_ID], rhs)

    switch.setup_switch(make.Ptemplate_, make.Pmqtt, config)

    if CONF_LAMBDA in config:
        template_ = None
        for template_ in process_lambda(config[CONF_LAMBDA], [],
                                        return_type=optional.template(bool_)):
            yield
        add(make.Ptemplate_.set_state_lambda(template_))
    if CONF_TURN_OFF_ACTION in config:
        automation.build_automation(make.Ptemplate_.get_turn_off_trigger(), NoArg,
                                    config[CONF_TURN_OFF_ACTION])
    if CONF_TURN_ON_ACTION in config:
        automation.build_automation(make.Ptemplate_.get_turn_on_trigger(), NoArg,
                                    config[CONF_TURN_ON_ACTION])
    if CONF_OPTIMISTIC in config:
        add(make.Ptemplate_.set_optimistic(config[CONF_OPTIMISTIC]))


BUILD_FLAGS = '-DUSE_TEMPLATE_SWITCH'
