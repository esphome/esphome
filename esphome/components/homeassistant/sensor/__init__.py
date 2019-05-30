import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ENTITY_ID, CONF_ID, ICON_EMPTY, UNIT_EMPTY
from .. import homeassistant_ns

DEPENDENCIES = ['api']

HomeassistantSensor = homeassistant_ns.class_('HomeassistantSensor', sensor.Sensor,
                                              cg.Component)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_EMPTY, ICON_EMPTY, 1).extend({
    cv.GenerateID(): cv.declare_id(HomeassistantSensor),
    cv.Required(CONF_ENTITY_ID): cv.entity_id,
})


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)

    cg.add(var.set_entity_id(config[CONF_ENTITY_ID]))
