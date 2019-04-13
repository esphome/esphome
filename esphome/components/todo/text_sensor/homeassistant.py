from esphome.components import text_sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ENTITY_ID, CONF_ID, CONF_NAME
DEPENDENCIES = ['api']

HomeassistantTextSensor = text_sensor.text_sensor_ns.class_('HomeassistantTextSensor',
                                                            text_sensor.TextSensor, Component)

PLATFORM_SCHEMA = cv.nameable(text_sensor.TEXT_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(HomeassistantTextSensor),
    cv.Required(CONF_ENTITY_ID): cv.entity_id,
}))


def to_code(config):
    rhs = App.make_homeassistant_text_sensor(config[CONF_NAME], config[CONF_ENTITY_ID])
    sensor_ = Pvariable(config[CONF_ID], rhs)
    text_sensor.setup_text_sensor(sensor_, config)


BUILD_FLAGS = '-DUSE_HOMEASSISTANT_TEXT_SENSOR'
