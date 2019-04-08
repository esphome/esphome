import voluptuous as vol

from esphome import automation
from esphome.components import binary_sensor, cover
import esphome.config_validation as cv
from esphome.const import CONF_CLOSE_ACTION, CONF_CLOSE_DURATION, \
    CONF_CLOSE_ENDSTOP, CONF_ID, CONF_NAME, CONF_OPEN_ACTION, CONF_OPEN_DURATION, \
    CONF_OPEN_ENDSTOP, CONF_STOP_ACTION, CONF_MAX_DURATION
from esphome.cpp_generator import Pvariable, add, get_variable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component

EndstopCover = cover.cover_ns.class_('EndstopCover', cover.Cover, Component)

PLATFORM_SCHEMA = cv.nameable(cover.COVER_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(EndstopCover),
    vol.Required(CONF_STOP_ACTION): automation.validate_automation(single=True),

    vol.Required(CONF_OPEN_ENDSTOP): cv.use_variable_id(binary_sensor.BinarySensor),
    vol.Required(CONF_OPEN_ACTION): automation.validate_automation(single=True),
    vol.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,

    vol.Required(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
    vol.Required(CONF_CLOSE_ENDSTOP): cv.use_variable_id(binary_sensor.BinarySensor),
    vol.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_MAX_DURATION): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.register_component(EndstopCover.new(config[CONF_NAME]))
    var = Pvariable(config[CONF_ID], rhs)
    cover.register_cover(var, config)
    setup_component(var, config)

    automation.build_automations(var.get_stop_trigger(), [],
                                 config[CONF_STOP_ACTION])

    for bin in get_variable(config[CONF_OPEN_ENDSTOP]):
        yield
    add(var.set_open_endstop(bin))
    add(var.set_open_duration(config[CONF_OPEN_DURATION]))
    automation.build_automations(var.get_open_trigger(), [],
                                 config[CONF_OPEN_ACTION])

    for bin in get_variable(config[CONF_CLOSE_ENDSTOP]):
        yield
    add(var.set_close_endstop(bin))
    add(var.set_close_duration(config[CONF_CLOSE_DURATION]))
    automation.build_automations(var.get_close_trigger(), [],
                                 config[CONF_CLOSE_ACTION])

    if CONF_MAX_DURATION in config:
        add(var.set_max_duration(config[CONF_MAX_DURATION]))


BUILD_FLAGS = '-DUSE_ENDSTOP_COVER'
