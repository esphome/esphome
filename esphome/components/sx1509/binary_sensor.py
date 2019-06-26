import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID
from esphome.components.sx1509.sensor import SX1509KeypadSensor, sx1509_ns

CONF_ROW = 'row'
CONF_COLUMN = 'col'


SX1509BinarySensor = sx1509_ns.class_('SX1509BinarySensor', binary_sensor.BinarySensor)

CONF_SX1509_KEYPAD_ID = 'sx1509_keypad_id'

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(SX1509BinarySensor),
    cv.GenerateID(CONF_SX1509_KEYPAD_ID): cv.use_id(SX1509KeypadSensor),
    cv.Required(CONF_ROW): cv.int_range(min=0, max=4),
    cv.Required(CONF_COLUMN): cv.int_range(min=0, max=4),
})


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield binary_sensor.register_binary_sensor(var, config)
    hub = yield cg.get_variable(config[CONF_SX1509_KEYPAD_ID])
    cg.add(var.set_row_col(config[CONF_ROW], config[CONF_COLUMN]))

    cg.add(hub.register_binary_sensor(var))
