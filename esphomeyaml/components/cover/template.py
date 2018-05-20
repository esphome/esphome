import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import automation
from esphomeyaml.components import cover
from esphomeyaml.const import CONF_CLOSE_ACTION, CONF_LAMBDA, CONF_MAKE_ID, CONF_NAME, \
    CONF_OPEN_ACTION, CONF_STOP_ACTION
from esphomeyaml.helpers import App, Application, NoArg, add, process_lambda, variable

PLATFORM_SCHEMA = cover.PLATFORM_SCHEMA.extend({
    cv.GenerateID('template_cover', CONF_MAKE_ID): cv.register_variable_id,
    vol.Required(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_OPEN_ACTION): automation.ACTIONS_SCHEMA,
    vol.Optional(CONF_CLOSE_ACTION): automation.ACTIONS_SCHEMA,
    vol.Optional(CONF_STOP_ACTION): automation.ACTIONS_SCHEMA,
}).extend(cover.COVER_SCHEMA.schema)

MakeTemplateCover = Application.MakeTemplateCover


def to_code(config):
    template_ = process_lambda(config[CONF_LAMBDA], [])
    rhs = App.make_template_cover(config[CONF_NAME], template_)
    make = variable(MakeTemplateCover, config[CONF_MAKE_ID], rhs)

    if CONF_OPEN_ACTION in config:
        actions = automation.build_actions(config[CONF_OPEN_ACTION], NoArg)
        add(make.Ptemplate_.add_open_actions(actions))
    if CONF_CLOSE_ACTION in config:
        actions = automation.build_actions(config[CONF_CLOSE_ACTION], NoArg)
        add(make.Ptemplate_.add_close_actions(actions))
    if CONF_STOP_ACTION in config:
        actions = automation.build_actions(config[CONF_STOP_ACTION], NoArg)
        add(make.Ptemplate_.add_stop_actions(actions))

    cover.setup_cover(make.Ptemplate_, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_TEMPLATE_COVER'
