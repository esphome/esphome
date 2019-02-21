import voluptuous as vol

from esphome.components import binary_sensor, sensor
from esphome.components.mpr121 import MPR121, CONF_MPR121_ID
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_NAME
from esphome.cpp_generator import get_variable

DEPENDENCIES = ['mpr121']
MPR121_Channel = sensor.sensor_ns.class_(
    'MPR121_Channel', binary_sensor.BinarySensor)

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MPR121_Channel),
    cv.GenerateID(CONF_MPR121_ID): cv.use_variable_id(MPR121),
    vol.Required(CONF_CHANNEL): vol.All(vol.Coerce(int), vol.Range(min=0, max=11))
}))

def func(full_config):
    return MPR121_Channel.new(full_config[CONF_NAME], full_config[CONF_CHANNEL])

def to_code(config):
    for hub in get_variable(config[CONF_MPR121_ID]):
        yield
    rhs = func(config)
    binary_sensor.register_binary_sensor(hub.add_channel(rhs), config)


def to_hass_config(data, config):
    return binary_sensor.core_to_hass_config(data, config)
