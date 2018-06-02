import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import automation
from esphomeyaml.components import cover
from esphomeyaml.const import CONF_CLOSE_ACTION, CONF_LAMBDA, CONF_MAKE_ID, CONF_NAME, \
    CONF_OPEN_ACTION, CONF_STOP_ACTION, CONF_OPTIMISTIC
from esphomeyaml.helpers import App, Application, NoArg, add, process_lambda, variable

MakeTemplateCover = Application.MakeTemplateCover

PLATFORM_SCHEMA = vol.All(cover.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeTemplateCover),
    vol.Optional(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_OPTIMISTIC): cv.boolean,
    vol.Optional(CONF_OPEN_ACTION): automation.ACTIONS_SCHEMA,
    vol.Optional(CONF_CLOSE_ACTION): automation.ACTIONS_SCHEMA,
    vol.Optional(CONF_STOP_ACTION): automation.ACTIONS_SCHEMA,
}).extend(cover.COVER_SCHEMA.schema), cv.has_at_least_one_key(CONF_LAMBDA, CONF_OPTIMISTIC))


def to_code(config):
    rhs = App.make_template_cover(config[CONF_NAME])
    make = variable(config[CONF_MAKE_ID], rhs)

    if CONF_LAMBDA in config:
        template_ = None
        for template_ in process_lambda(config[CONF_LAMBDA], []):
            yield
        add(make.Ptemplate_.set_state_lambda(template_))
    if CONF_OPEN_ACTION in config:
        actions = None
        for actions in automation.build_actions(config[CONF_OPEN_ACTION], NoArg):
            yield
        add(make.Ptemplate_.add_open_actions(actions))
    if CONF_CLOSE_ACTION in config:
        actions = None
        for actions in automation.build_actions(config[CONF_CLOSE_ACTION], NoArg):
            yield
        add(make.Ptemplate_.add_close_actions(actions))
    if CONF_STOP_ACTION in config:
        actions = None
        for actions in automation.build_actions(config[CONF_STOP_ACTION], NoArg):
            yield
        add(make.Ptemplate_.add_stop_actions(actions))
    if CONF_OPTIMISTIC in config:
        add(make.Ptemplate_.set_optimistic(config[CONF_OPTIMISTIC]))

    cover.setup_cover(make.Ptemplate_, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_TEMPLATE_COVER'
