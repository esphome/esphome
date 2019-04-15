from esphome.components import text_sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ENTITY_ID, CONF_ID, CONF_NAME
from .. import homeassistant_ns

DEPENDENCIES = ['api']

HomeassistantTextSensor = homeassistant_ns.class_('HomeassistantTextSensor',
                                                  text_sensor.TextSensor, cg.Component)

CONFIG_SCHEMA = cv.nameable(text_sensor.TEXT_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(HomeassistantTextSensor),
    cv.Required(CONF_ENTITY_ID): cv.entity_id,
}))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], config[CONF_ENTITY_ID])
    yield cg.register_component(var, config)
    yield text_sensor.register_text_sensor(var, config)
