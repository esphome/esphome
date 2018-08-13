import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import automation
from esphomeyaml.components import cover
from esphomeyaml.const import CONF_CLOSE_ACTION, CONF_LAMBDA, CONF_MAKE_ID, CONF_NAME, \
    CONF_OPEN_ACTION, CONF_STOP_ACTION, CONF_OPTIMISTIC
from esphomeyaml.helpers import App, Application, NoArg, add, process_lambda, variable, optional

MakeTemplateCover = Application.MakeTemplateCover

PLATFORM_SCHEMA = cv.nameable(cover.COVER_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeTemplateCover),
    vol.Optional(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_OPTIMISTIC): cv.boolean,
    vol.Optional(CONF_OPEN_ACTION): automation.validate_automation(),
    vol.Optional(CONF_CLOSE_ACTION): automation.validate_automation(),
    vol.Optional(CONF_STOP_ACTION): automation.validate_automation(),
}), cv.has_at_least_one_key(CONF_LAMBDA, CONF_OPTIMISTIC))


def to_code(config):
    rhs = App.make_template_cover(config[CONF_NAME])
    make = variable(config[CONF_MAKE_ID], rhs)

    cover.setup_cover(make.Ptemplate_, make.Pmqtt, config)

    if CONF_LAMBDA in config:
        template_ = None
        for template_ in process_lambda(config[CONF_LAMBDA], [],
                                        return_type=optional.template(cover.CoverState)):
            yield
        add(make.Ptemplate_.set_state_lambda(template_))
    if CONF_OPEN_ACTION in config:
        automation.build_automation(make.Ptemplate_.get_open_trigger(), NoArg,
                                    config[CONF_OPEN_ACTION])
    if CONF_CLOSE_ACTION in config:
        automation.build_automation(make.Ptemplate_.get_close_trigger(), NoArg,
                                    config[CONF_CLOSE_ACTION])
    if CONF_STOP_ACTION in config:
        automation.build_automation(make.Ptemplate_.get_stop_trigger(), NoArg,
                                    config[CONF_STOP_ACTION])
    if CONF_OPTIMISTIC in config:
        add(make.Ptemplate_.set_optimistic(config[CONF_OPTIMISTIC]))


BUILD_FLAGS = '-DUSE_TEMPLATE_COVER'
