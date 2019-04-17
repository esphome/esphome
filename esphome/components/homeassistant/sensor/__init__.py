import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ENTITY_ID, CONF_ID, CONF_NAME
from .. import homeassistant_ns

DEPENDENCIES = ['api']

HomeassistantSensor = homeassistant_ns.class_('HomeassistantSensor', sensor.Sensor)

CONFIG_SCHEMA = cv.nameable(sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(HomeassistantSensor),
    cv.Required(CONF_ENTITY_ID): cv.entity_id,
}))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], config[CONF_ENTITY_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
