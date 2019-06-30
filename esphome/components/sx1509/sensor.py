import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, UNIT_EMPTY, ICON_EMPTY
from . import SX1509Component, sx1509_ns

CONF_KEY_ROWS = 'key_rows'
CONF_KEY_COLUMNS = 'key_columns'
CONF_SLEEP_TIME = 'sleep_time'
CONF_SCAN_TIME = 'scan_time'
CONF_DEBOUNCE_TIME = 'debounce_time'

DEPENDENCIES = ['sx1509']

SX1509KeypadSensor = sx1509_ns.class_('SX1509KeypadSensor', sensor.Sensor, cg.Component)

CONF_SX1509_ID = 'sx1509_id'

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_EMPTY, ICON_EMPTY, 0).extend({
    cv.GenerateID(): cv.declare_id(SX1509KeypadSensor),
    cv.GenerateID(CONF_SX1509_ID): cv.use_id(SX1509Component),
    cv.Required(CONF_KEY_ROWS): cv.int_range(min=0, max=7),
    cv.Required(CONF_KEY_COLUMNS): cv.int_range(min=0, max=7),
    cv.Optional(CONF_SLEEP_TIME): cv.int_range(min=128, max=8192),
    cv.Optional(CONF_SCAN_TIME): cv.int_range(min=1, max=128),
    cv.Optional(CONF_DEBOUNCE_TIME): cv.int_range(min=1, max=64),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    parent = yield cg.get_variable(config[CONF_SX1509_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
    cg.add(var.set_parent(parent))

    cg.add(var.set_rows_cols(config[CONF_KEY_ROWS], config[CONF_KEY_COLUMNS]))
    cg.add(var.set_timers(config[CONF_SLEEP_TIME], config[CONF_SCAN_TIME],
                          config[CONF_DEBOUNCE_TIME]))
