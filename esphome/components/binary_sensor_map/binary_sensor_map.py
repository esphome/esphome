import voluptuous as vol

from esphome.components import sensor, binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_CHANNELS, CONF_CHANNEL, CONF_VALUE, CONF_TYPE
from esphome.cpp_generator import Pvariable, get_variable, add
from esphome.cpp_types import App
from esphome.components.sensor import setup_sensor

DEPENDENCIES = ['binary_sensor']

BinarySensorMap = sensor.sensor_ns.class_('BinarySensorMap', sensor.Sensor)

SensorMapType = sensor.sensor_ns.enum('SensorMapType')
SENSOR_MAP_TYPES = {
    'GROUP': SensorMapType.BINARY_SENSOR_MAP_TYPE_GROUP,
}

entry = {
    vol.Required(CONF_CHANNEL): cv.use_variable_id(binary_sensor.BinarySensor),
    vol.Required(CONF_VALUE): vol.All(cv.positive_int, vol.Range(min=0, max=255)),
}

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(BinarySensorMap),
    vol.Required(CONF_TYPE): cv.one_of(*SENSOR_MAP_TYPES, upper=True),
    vol.Required(CONF_CHANNELS): cv.ensure_list(entry),
}))


def to_code(config):
    rhs = App.make_binary_sensor_map(config[CONF_NAME])
    var = Pvariable(config[CONF_ID], rhs)
    setup_sensor(var, config)
    if CONF_TYPE in config:
        constant = SENSOR_MAP_TYPES[config[CONF_TYPE]]
        add(var.set_sensor_type(constant))

    for ch in config[CONF_CHANNELS]:
        for input_var in get_variable(ch[CONF_CHANNEL]):
            yield
        add(var.add_sensor(input_var, ch[CONF_VALUE]))

    add(App.register_sensor(var))


BUILD_FLAGS = '-DUSE_BINARY_SENSOR_MAP'