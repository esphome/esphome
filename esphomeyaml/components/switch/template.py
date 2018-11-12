import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import automation
from esphomeyaml.components import switch
from esphomeyaml.const import CONF_LAMBDA, CONF_MAKE_ID, CONF_NAME, CONF_TURN_OFF_ACTION, \
    CONF_TURN_ON_ACTION, CONF_OPTIMISTIC, CONF_RESTORE_STATE
from esphomeyaml.helpers import App, Application, process_lambda, variable, NoArg, add, bool_, \
    optional, setup_component, Component

MakeTemplateSwitch = Application.struct('MakeTemplateSwitch')
TemplateSwitch = switch.switch_ns.class_('TemplateSwitch', switch.Switch, Component)

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateSwitch),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeTemplateSwitch),
    vol.Optional(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_OPTIMISTIC): cv.boolean,
    vol.Optional(CONF_TURN_OFF_ACTION): automation.validate_automation(single=True),
    vol.Optional(CONF_TURN_ON_ACTION): automation.validate_automation(single=True),
    vol.Optional(CONF_RESTORE_STATE): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA.schema), cv.has_at_least_one_key(CONF_LAMBDA, CONF_OPTIMISTIC))


def to_code(config):
    rhs = App.make_template_switch(config[CONF_NAME])
    make = variable(config[CONF_MAKE_ID], rhs)
    template = make.Ptemplate_

    switch.setup_switch(template, make.Pmqtt, config)

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
