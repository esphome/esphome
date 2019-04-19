import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import cover
from esphome.const import CONF_CLOSE_ACTION, CONF_CLOSE_DURATION, CONF_ID, CONF_OPEN_ACTION, \
    CONF_OPEN_DURATION, CONF_STOP_ACTION

time_based_ns = cg.esphome_ns.namespace('time_based')
TimeBasedCover = time_based_ns.class_('TimeBasedCover', cover.Cover, cg.Component)

CONFIG_SCHEMA = cover.COVER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(TimeBasedCover),
    cv.Required(CONF_STOP_ACTION): automation.validate_automation(single=True),

    cv.Required(CONF_OPEN_ACTION): automation.validate_automation(single=True),
    cv.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,

    cv.Required(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
    cv.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield cover.register_cover(var, config)

    yield automation.build_automation(var.get_stop_trigger(), [], config[CONF_STOP_ACTION])

    cg.add(var.set_open_duration(config[CONF_OPEN_DURATION]))
    yield automation.build_automation(var.get_open_trigger(), [], config[CONF_OPEN_ACTION])

    cg.add(var.set_close_duration(config[CONF_CLOSE_DURATION]))
    yield automation.build_automation(var.get_close_trigger(), [], config[CONF_CLOSE_ACTION])
