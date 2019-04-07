import voluptuous as vol

from esphome import automation
from esphome.components import cover
import esphome.config_validation as cv
from esphome.const import CONF_CLOSE_ACTION, CONF_CLOSE_DURATION, CONF_ID, CONF_NAME, \
    CONF_OPEN_ACTION, CONF_OPEN_DURATION, CONF_STOP_ACTION
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component

TimeBasedCover = cover.cover_ns.class_('TimeBasedCover', cover.Cover, Component)

PLATFORM_SCHEMA = cv.nameable(cover.COVER_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TimeBasedCover),
    vol.Required(CONF_STOP_ACTION): automation.validate_automation(single=True),

    vol.Required(CONF_OPEN_ACTION): automation.validate_automation(single=True),
    vol.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,

    vol.Required(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
    vol.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.register_component(TimeBasedCover.new(config[CONF_NAME]))
    var = Pvariable(config[CONF_ID], rhs)
    cover.register_cover(var, config)
    setup_component(var, config)

    automation.build_automations(var.get_stop_trigger(), [],
                                 config[CONF_STOP_ACTION])

    add(var.set_open_duration(config[CONF_OPEN_DURATION]))
    automation.build_automations(var.get_open_trigger(), [],
                                 config[CONF_OPEN_ACTION])

    add(var.set_close_duration(config[CONF_CLOSE_DURATION]))
    automation.build_automations(var.get_close_trigger(), [],
                                 config[CONF_CLOSE_ACTION])


BUILD_FLAGS = '-DUSE_TIME_BASED_COVER'
