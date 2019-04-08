import voluptuous as vol

from esphome import automation
from esphome.automation import ACTION_REGISTRY
from esphome.components import cover
import esphome.config_validation as cv
from esphome.const import CONF_ASSUMED_STATE, CONF_CLOSE_ACTION, CONF_ID, CONF_LAMBDA, CONF_NAME, \
    CONF_OPEN_ACTION, CONF_OPTIMISTIC, CONF_STATE, CONF_STOP_ACTION, CONF_POSITION, \
    CONF_CURRENT_OPERATION, CONF_RESTORE_MODE
from esphome.cpp_generator import Pvariable, add, get_variable, process_lambda, templatable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import Action, App, optional

TemplateCover = cover.cover_ns.class_('TemplateCover', cover.Cover)
CoverPublishAction = cover.cover_ns.class_('CoverPublishAction', Action)

TemplateCoverRestoreMode = cover.cover_ns.enum('TemplateCoverRestoreMode')
RESTORE_MODES = {
    'NO_RESTORE': TemplateCoverRestoreMode.NO_RESTORE,
    'RESTORE': TemplateCoverRestoreMode.RESTORE,
    'RESTORE_AND_CALL': TemplateCoverRestoreMode.RESTORE_AND_CALL,
}

PLATFORM_SCHEMA = cv.nameable(cover.COVER_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateCover),
    vol.Optional(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_OPTIMISTIC): cv.boolean,
    vol.Optional(CONF_ASSUMED_STATE): cv.boolean,
    vol.Optional(CONF_OPEN_ACTION): automation.validate_automation(single=True),
    vol.Optional(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
    vol.Optional(CONF_STOP_ACTION): automation.validate_automation(single=True),
    vol.Optional(CONF_RESTORE_MODE): cv.one_of(*RESTORE_MODES, upper=True),
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.register_component(TemplateCover.new(config[CONF_NAME]))
    var = Pvariable(config[CONF_ID], rhs)
    cover.register_cover(var, config)
    setup_component(var, config)

    if CONF_LAMBDA in config:
        for template_ in process_lambda(config[CONF_LAMBDA], [],
                                        return_type=optional.template(float)):
            yield
        add(var.set_state_lambda(template_))
    if CONF_OPEN_ACTION in config:
        automation.build_automations(var.get_open_trigger(), [],
                                     config[CONF_OPEN_ACTION])
    if CONF_CLOSE_ACTION in config:
        automation.build_automations(var.get_close_trigger(), [],
                                     config[CONF_CLOSE_ACTION])
    if CONF_STOP_ACTION in config:
        automation.build_automations(var.get_stop_trigger(), [],
                                     config[CONF_STOP_ACTION])
    if CONF_OPTIMISTIC in config:
        add(var.set_optimistic(config[CONF_OPTIMISTIC]))
    if CONF_ASSUMED_STATE in config:
        add(var.set_assumed_state(config[CONF_ASSUMED_STATE]))
    if CONF_RESTORE_MODE in config:
        add(var.set_restore_mode(RESTORE_MODES[config[CONF_RESTORE_MODE]]))


BUILD_FLAGS = '-DUSE_TEMPLATE_COVER'

CONF_COVER_TEMPLATE_PUBLISH = 'cover.template.publish'
COVER_TEMPLATE_PUBLISH_ACTION_SCHEMA = cv.Schema({
    vol.Required(CONF_ID): cv.use_variable_id(cover.Cover),
    vol.Exclusive(CONF_STATE, 'pos'): cv.templatable(cover.validate_cover_state),
    vol.Exclusive(CONF_POSITION, 'pos'): cv.templatable(cv.zero_to_one_float),
    vol.Optional(CONF_CURRENT_OPERATION): cv.templatable(cover.validate_cover_operation),
})


@ACTION_REGISTRY.register(CONF_COVER_TEMPLATE_PUBLISH,
                          COVER_TEMPLATE_PUBLISH_ACTION_SCHEMA)
def cover_template_publish_to_code(config, action_id, template_arg, args):
    for var in get_variable(config[CONF_ID]):
        yield None
    type = CoverPublishAction.template(template_arg)
    rhs = type.new(var)
    action = Pvariable(action_id, rhs, type=type)
    if CONF_STATE in config:
        for template_ in templatable(config[CONF_STATE], args, float,
                                     to_exp=cover.COVER_STATES):
            yield None
        add(action.set_position(template_))
    if CONF_POSITION in config:
        for template_ in templatable(config[CONF_POSITION], args, float,
                                     to_exp=cover.COVER_STATES):
            yield None
        add(action.set_position(template_))
    if CONF_CURRENT_OPERATION in config:
        for template_ in templatable(config[CONF_CURRENT_OPERATION], args, cover.CoverOperation,
                                     to_exp=cover.COVER_OPERATIONS):
            yield None
        add(action.set_current_operation(template_))
    yield action
