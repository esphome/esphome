import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ATTRIBUTE, CONF_ENTITY_ID
from .. import homeassistant_ns

DEPENDENCIES = ["api"]
HomeassistantBinarySensor = homeassistant_ns.class_(
    "HomeassistantBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(HomeassistantBinarySensor)
    .extend(
        {
            cv.Required(CONF_ENTITY_ID): cv.entity_id,
            cv.Optional(CONF_ATTRIBUTE): cv.string,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)

    cg.add(var.set_entity_id(config[CONF_ENTITY_ID]))
    if CONF_ATTRIBUTE in config:
        cg.add(var.set_attribute(config[CONF_ATTRIBUTE]))
