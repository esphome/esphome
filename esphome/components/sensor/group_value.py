import voluptuous as vol

from esphome.components import sensor, binary_sensor
import esphome.config_validation as cv
from esphome.const import  CONF_ID, CONF_NAME, CONF_CHANNELS, CONF_CHANNEL, CONF_VALUE
from esphome.cpp_generator import Pvariable, get_variable, variable, add
from esphome.cpp_types import App, Component
from array import array
from const import CONF_ID
from test.test_multiprocessing import get_value
from esphome.components.sensor import setup_sensor

DEPENDENCIES = ['binary_sensor']
MULTI_CONF = True

GroupSensorComponent = sensor.sensor_ns.class_('GroupSensorComponent', sensor.Sensor)

entry = {
    vol.Required(CONF_CHANNEL): cv.use_variable_id(binary_sensor.BinarySensor),
    vol.Required(CONF_VALUE): vol.All(cv.positive_int, vol.Range(min=0, max=255)),
}

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(GroupSensorComponent),
    vol.Required(CONF_CHANNELS): vol.All([entry])
}))


def to_code(config):
    rhs = App.make_group_sensor(config[CONF_NAME])
    var = Pvariable(config[CONF_ID], rhs)
    setup_sensor(var, config)
    
    for ch in config[CONF_CHANNELS]:
        for input_var in  get_variable(ch[CONF_CHANNEL]):
            yield
        add(var.add_sensor(input_var, ch[CONF_VALUE]))
          
    add(App.register_sensor(var))


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)


BUILD_FLAGS = '-DUSE_GROUP_SENSOR'
