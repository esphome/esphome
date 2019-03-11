import voluptuous as vol

from esphome.components import sensor
from esphome.components.mpr121 import MPR121Component, CONF_MPR121_ID
import esphome.config_validation as cv
from esphome.const import  CONF_NAME, CONF_CHANNELS, CONF_CHANNEL, CONF_VALUE, CONF_TYPE, CONF_STEP_SIZE
from esphome.cpp_generator import Pvariable, get_variable, add
from esphome.cpp_types import App, Component
from array import array
from const import CONF_ID

GroupSensorComponent = sensor.sensor_ns.class_('GroupSensorComponent', sensor.Sensor)

entry = {
    cv.GenerateID(): cv.declare_variable_id(CONF_MPR121_ID),
    vol.Required(CONF_CHANNEL): vol.All(cv.positive_int, vol.Range(min=0, max=11)),
    vol.Optional(CONF_VALUE): vol.All(cv.positive_int, vol.Range(min=0, max=255)),
}

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(GroupSensorComponent),
    cv.GenerateID(CONF_MPR121_ID): cv.use_variable_id(MPR121Component),
    vol.Required(CONF_CHANNELS): vol.All([entry], vol.Length(min=1, max=12))
}))


def to_code(config):
    for hub in get_variable(config[CONF_MPR121_ID]):
        yield
    sensh = hub.make_sensor(config[CONF_NAME])    
    mysensor = Pvariable(config[CONF_ID], sensh)
    for ch in config[CONF_CHANNELS]:
        add(mysensor.add_sensor_channel(ch[CONF_CHANNEL], ch[CONF_VALUE]))
    add(App.register_sensor(mysensor))


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
