import voluptuous as vol

from esphomeyaml.components import sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ENTITY_ID, CONF_MAKE_ID, CONF_NAME
from esphomeyaml.cpp_generator import variable
from esphomeyaml.cpp_types import App, Application

DEPENDENCIES = ['api']

MakeHomeassistantSensor = Application.struct('MakeHomeassistantSensor')
HomeassistantSensor = sensor.sensor_ns.class_('HomeassistantSensor', sensor.Sensor)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(HomeassistantSensor),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeHomeassistantSensor),
    vol.Required(CONF_ENTITY_ID): cv.entity_id,
}))


def to_code(config):
    rhs = App.make_homeassistant_sensor(config[CONF_NAME], config[CONF_ENTITY_ID])
    make = variable(config[CONF_MAKE_ID], rhs)
    subs = make.Psensor
    sensor.setup_sensor(subs, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_HOMEASSISTANT_SENSOR'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
