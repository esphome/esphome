from esphome.components import binary_sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ENTITY_ID, CONF_ID, CONF_NAME
from .. import homeassistant_ns

DEPENDENCIES = ['api']
HomeassistantBinarySensor = homeassistant_ns.class_('HomeassistantBinarySensor',
                                                    binary_sensor.BinarySensor)

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(HomeassistantBinarySensor),
    cv.Required(CONF_ENTITY_ID): cv.entity_id,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], config[CONF_ENTITY_ID])
    yield cg.register_component(var, config)
    yield binary_sensor.register_binary_sensor(var, config)
