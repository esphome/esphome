import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ENTITY_ID, CONF_ID
from .. import homeassistant_ns

DEPENDENCIES = ['api']

HomeassistantTextSensor = homeassistant_ns.class_('HomeassistantTextSensor',
                                                  text_sensor.TextSensor, cg.Component)

CONFIG_SCHEMA = text_sensor.TEXT_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(HomeassistantTextSensor),
    cv.Required(CONF_ENTITY_ID): cv.entity_id,
})


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield text_sensor.register_text_sensor(var, config)

    cg.add(var.set_entity_id(config[CONF_ENTITY_ID]))
