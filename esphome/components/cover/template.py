import voluptuous as vol

from esphome import automation
from esphome.automation import ACTION_REGISTRY
from esphome.components import cover
import esphome.config_validation as cv
from esphome.const import CONF_ASSUMED_STATE, CONF_CLOSE_ACTION, CONF_ID, CONF_LAMBDA, CONF_NAME, \
    CONF_OPEN_ACTION, CONF_OPTIMISTIC, CONF_STATE, CONF_STOP_ACTION
from esphome.cpp_generator import Pvariable, add, get_variable, process_lambda, templatable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import Action, App, NoArg, optional
from esphome.py_compat import string_types

TemplateCover = cover.cover_ns.class_('TemplateCover', cover.Cover)
CoverPublishAction = cover.cover_ns.class_('CoverPublishAction', Action)

PLATFORM_SCHEMA = cv.nameable(cover.COVER_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateCover),
    vol.Optional(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_OPTIMISTIC): cv.boolean,
    vol.Optional(CONF_ASSUMED_STATE): cv.boolean,
    vol.Optional(CONF_OPEN_ACTION): automation.validate_automation(single=True),
    vol.Optional(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
    vol.Optional(CONF_STOP_ACTION): automation.validate_automation(single=True),
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_template_cover(config[CONF_NAME])
    var = Pvariable(config[CONF_ID], rhs)

    cover.setup_cover(var, config)
    setup_component(var, config)

    if CONF_LAMBDA in config:
        for template_ in process_lambda(config[CONF_LAMBDA], [],
                                        return_type=optional.template(cover.CoverState)):
            yield
        add(var.set_state_lambda(template_))
    if CONF_OPEN_ACTION in config:
        automation.build_automation(var.get_open_trigger(), NoArg,
                                    config[CONF_OPEN_ACTION])
    if CONF_CLOSE_ACTION in config:
        automation.build_automation(var.get_close_trigger(), NoArg,
                                    config[CONF_CLOSE_ACTION])
    if CONF_STOP_ACTION in config:
        automation.build_automation(var.get_stop_trigger(), NoArg,
                                    config[CONF_STOP_ACTION])
    if CONF_OPTIMISTIC in config:
        add(var.set_optimistic(config[CONF_OPTIMISTIC]))
    if CONF_ASSUMED_STATE in config:
        add(var.set_assumed_state(config[CONF_ASSUMED_STATE]))


BUILD_FLAGS = '-DUSE_TEMPLATE_COVER'

CONF_COVER_TEMPLATE_PUBLISH = 'cover.template.publish'
COVER_TEMPLATE_PUBLISH_ACTION_SCHEMA = vol.Schema({
    vol.Required(CONF_ID): cv.use_variable_id(cover.Cover),
    vol.Required(CONF_STATE): cv.templatable(cover.validate_cover_state),
})


@ACTION_REGISTRY.register(CONF_COVER_TEMPLATE_PUBLISH,
                          COVER_TEMPLATE_PUBLISH_ACTION_SCHEMA)
def cover_template_publish_to_code(config, action_id, arg_type, template_arg):
    for var in get_variable(config[CONF_ID]):
        yield None
    rhs = var.make_cover_publish_action(template_arg)
    type = CoverPublishAction.template(arg_type)
    action = Pvariable(action_id, rhs, type=type)
    state = config[CONF_STATE]
    if isinstance(state, string_types):
        template_ = cover.COVER_STATES[state]
    else:
        for template_ in templatable(state, arg_type, cover.CoverState):
            yield None
    add(action.set_state(template_))
    yield action


def to_hass_config(data, config):
    ret = cover.core_to_hass_config(data, config)
    if ret is None:
        return None
    if CONF_OPTIMISTIC in config:
        ret['optimistic'] = config[CONF_OPTIMISTIC]
    return ret
