import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sensor, cover
from esphome.const import CONF_CLOSE_ACTION, CONF_CLOSE_DURATION, \
    CONF_ID, CONF_OPEN_ACTION, CONF_OPEN_DURATION, \
    CONF_STOP_ACTION, CONF_MAX_DURATION


CONF_OPEN_SENSOR = 'open_sensor'
CONF_OPEN_MOVING_CURRENT_THRESHOLD = "open_moving_current_threshold"
CONF_OPEN_OBSTACLE_CURRENT_THRESHOLD = "open_obstacle_current_threshold"

CONF_CLOSE_SENSOR = 'close_sensor'
CONF_CLOSE_MOVING_CURRENT_THRESHOLD = "close_moving_current_threshold"
CONF_CLOSE_OBSTACLE_CURRENT_THRESHOLD = "close_obstacle_current_threshold"

CONF_OBSTACLE_ROLLBACK = "obstacle_rollback"
CONF_MALFUNCTION_DETECTION = "malfunction_detection"
CONF_MALFUNCTION_ACTION = "malfunction_action"
CONF_START_SENSING_DELAY = "start_sensing_delay"

current_based_ns = cg.esphome_ns.namespace('current_based')
CurrentBasedCover = current_based_ns.class_('CurrentBasedCover', cover.Cover, cg.Component)

CONFIG_SCHEMA = cover.COVER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(CurrentBasedCover),
    cv.Required(CONF_STOP_ACTION): automation.validate_automation(single=True),

    cv.Required(CONF_OPEN_SENSOR): cv.use_id(sensor.Sensor),
    cv.Required(CONF_OPEN_MOVING_CURRENT_THRESHOLD): cv.float_range(min=0, min_included=False),
    cv.Optional(CONF_OPEN_OBSTACLE_CURRENT_THRESHOLD): cv.float_range(min=0, min_included=False),
    cv.Required(CONF_OPEN_ACTION): automation.validate_automation(single=True),
    cv.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,

    cv.Required(CONF_CLOSE_SENSOR): cv.use_id(sensor.Sensor),
    cv.Required(CONF_CLOSE_MOVING_CURRENT_THRESHOLD): cv.float_range(min=0, min_included=False),
    cv.Optional(CONF_CLOSE_OBSTACLE_CURRENT_THRESHOLD): cv.float_range(min=0, min_included=False),
    cv.Required(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
    cv.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,

    cv.Optional(CONF_OBSTACLE_ROLLBACK, default="10%"): cv.percentage,
    cv.Optional(CONF_MAX_DURATION): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_MALFUNCTION_DETECTION, default=True): cv.boolean,
    cv.Optional(CONF_MALFUNCTION_ACTION): automation.validate_automation(single=True),
    cv.Optional(CONF_START_SENSING_DELAY, default='500ms'): cv.positive_time_period_milliseconds
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield cover.register_cover(var, config)

    yield automation.build_automation(var.get_stop_trigger(), [], config[CONF_STOP_ACTION])

    # OPEN
    bin = yield cg.get_variable(config[CONF_OPEN_SENSOR])
    cg.add(var.set_open_sensor(bin))
    cg.add(var.set_open_moving_current_threshold(config[CONF_OPEN_MOVING_CURRENT_THRESHOLD]))
    if CONF_OPEN_OBSTACLE_CURRENT_THRESHOLD in config:
        cg.add(var.set_open_obstacle_current_threshold(config[CONF_OPEN_OBSTACLE_CURRENT_THRESHOLD]))
    cg.add(var.set_open_duration(config[CONF_OPEN_DURATION]))
    yield automation.build_automation(var.get_open_trigger(), [], config[CONF_OPEN_ACTION])

    # CLOSE
    bin = yield cg.get_variable(config[CONF_CLOSE_SENSOR])
    cg.add(var.set_close_sensor(bin))
    cg.add(var.set_close_moving_current_threshold(config[CONF_CLOSE_MOVING_CURRENT_THRESHOLD]))
    if CONF_CLOSE_OBSTACLE_CURRENT_THRESHOLD in config:
        cg.add(var.set_close_obstacle_current_threshold(config[CONF_CLOSE_OBSTACLE_CURRENT_THRESHOLD]))
    cg.add(var.set_close_duration(config[CONF_CLOSE_DURATION]))
    yield automation.build_automation(var.get_close_trigger(), [], config[CONF_CLOSE_ACTION])

    cg.add(var.set_obstacle_rollback(config[CONF_OBSTACLE_ROLLBACK]))
    if CONF_MAX_DURATION in config:
        cg.add(var.set_max_duration(config[CONF_MAX_DURATION]))
    cg.add(var.set_malfunction_detection(config[CONF_MALFUNCTION_DETECTION]))
    if CONF_MALFUNCTION_ACTION in config:
        yield automation.build_automation(var.get_malfunction_trigger(), [], config[CONF_MALFUNCTION_ACTION])
    cg.add(var.set_start_sensing_delay(config[CONF_START_SENSING_DELAY]))
