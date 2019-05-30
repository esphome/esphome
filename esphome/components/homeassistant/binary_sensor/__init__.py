import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ENTITY_ID, CONF_ID
from .. import homeassistant_ns

DEPENDENCIES = ['api']
HomeassistantBinarySensor = homeassistant_ns.class_('HomeassistantBinarySensor',
                                                    binary_sensor.BinarySensor,
                                                    cg.Component)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(HomeassistantBinarySensor),
    cv.Required(CONF_ENTITY_ID): cv.entity_id,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield binary_sensor.register_binary_sensor(var, config)

    cg.add(var.set_entity_id(config[CONF_ENTITY_ID]))
