import voluptuous as vol

from esphomeyaml.components import text_sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ENTITY_ID, CONF_MAKE_ID, CONF_NAME
from esphomeyaml.cpp_generator import variable
from esphomeyaml.cpp_types import App, Application, Component

DEPENDENCIES = ['api']

MakeHomeassistantTextSensor = Application.struct('MakeHomeassistantTextSensor')
HomeassistantTextSensor = text_sensor.text_sensor_ns.class_('HomeassistantTextSensor',
                                                            text_sensor.TextSensor, Component)

PLATFORM_SCHEMA = cv.nameable(text_sensor.TEXT_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(HomeassistantTextSensor),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeHomeassistantTextSensor),
    vol.Required(CONF_ENTITY_ID): cv.entity_id,
}))


def to_code(config):
    rhs = App.make_homeassistant_text_sensor(config[CONF_NAME], config[CONF_ENTITY_ID])
    make = variable(config[CONF_MAKE_ID], rhs)
    sensor_ = make.Psensor
    text_sensor.setup_text_sensor(sensor_, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_HOMEASSISTANT_TEXT_SENSOR'


def to_hass_config(data, config):
    return text_sensor.core_to_hass_config(data, config)
