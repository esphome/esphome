import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import automation
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_LAMBDA, CONF_MAKE_ID, CONF_NAME, CONF_TURN_OFF_ACTION, \
    CONF_TURN_ON_ACTION
from esphomeyaml.helpers import App, Application, process_lambda, variable, NoArg, add

PLATFORM_SCHEMA = switch.PLATFORM_SCHEMA.extend({
    cv.GenerateID('template_switch', CONF_MAKE_ID): cv.register_variable_id,
    vol.Required(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_TURN_OFF_ACTION): automation.ACTIONS_SCHEMA,
    vol.Optional(CONF_TURN_ON_ACTION): automation.ACTIONS_SCHEMA,
}).extend(switch.SWITCH_SCHEMA.schema)

MakeTemplateSwitch = Application.MakeTemplateSwitch


def to_code(config):
    template_ = process_lambda(config[CONF_LAMBDA], [])
    rhs = App.make_template_switch(config[CONF_NAME], template_)
    make = variable(MakeTemplateSwitch, config[CONF_MAKE_ID], rhs)

    if CONF_TURN_OFF_ACTION in config:
        actions = automation.build_actions(config[CONF_TURN_OFF_ACTION], NoArg)
        add(make.Ptemplate_.add_turn_off_actions(actions))
    if CONF_TURN_ON_ACTION in config:
        actions = automation.build_actions(config[CONF_TURN_ON_ACTION], NoArg)
        add(make.Ptemplate_.add_turn_on_actions(actions))

    switch.setup_switch(make.Ptemplate_, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_TEMPLATE_SWITCH'
