import voluptuous as vol

from esphome import automation
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_OPTIMISTIC, \
    CONF_RESTORE_STATE, CONF_TURN_OFF_ACTION, CONF_TURN_ON_ACTION
from esphome.cpp_generator import Pvariable, add, process_lambda
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component, NoArg, bool_, optional

TemplateSwitch = switch.switch_ns.class_('TemplateSwitch', switch.Switch, Component)

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateSwitch),
    vol.Optional(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_OPTIMISTIC): cv.boolean,
    vol.Optional(CONF_TURN_OFF_ACTION): automation.validate_automation(single=True),
    vol.Optional(CONF_TURN_ON_ACTION): automation.validate_automation(single=True),
    vol.Optional(CONF_RESTORE_STATE): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA.schema), cv.has_at_least_one_key(CONF_LAMBDA, CONF_OPTIMISTIC))


def to_code(config):
    rhs = App.make_template_switch(config[CONF_NAME])
    template = Pvariable(config[CONF_ID], rhs)

    switch.setup_switch(template, config)

    if CONF_LAMBDA in config:
        for template_ in process_lambda(config[CONF_LAMBDA], [],
                                        return_type=optional.template(bool_)):
            yield
        add(template.set_state_lambda(template_))
    if CONF_TURN_OFF_ACTION in config:
        automation.build_automation(template.get_turn_off_trigger(), NoArg,
                                    config[CONF_TURN_OFF_ACTION])
    if CONF_TURN_ON_ACTION in config:
        automation.build_automation(template.get_turn_on_trigger(), NoArg,
                                    config[CONF_TURN_ON_ACTION])
    if CONF_OPTIMISTIC in config:
        add(template.set_optimistic(config[CONF_OPTIMISTIC]))

    if CONF_RESTORE_STATE in config:
        add(template.set_restore_state(config[CONF_RESTORE_STATE]))

    setup_component(template, config)


BUILD_FLAGS = '-DUSE_TEMPLATE_SWITCH'


def to_hass_config(data, config):
    return switch.core_to_hass_config(data, config)
