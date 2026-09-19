import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, DEVICE_CLASS_CONNECTIVITY, ENTITY_CATEGORY_DIAGNOSTIC

from . import (
    ECOCOMFORT2_CLIENT_SCHEMA,
    Ecocomfort2BinarySensor,
    register_ecocomfort2_child,
)
from .const import CONF_BOOST, CONF_CONNECTED

CODEOWNERS = ["@gledian"]
DEPENDENCIES = ["ecocomfort2"]

SENSOR_TYPES = {
    CONF_CONNECTED: binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_CONNECTIVITY,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_BOOST: binary_sensor.binary_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Ecocomfort2BinarySensor),
        }
    )
    .extend({cv.Optional(type_): schema for type_, schema in SENSOR_TYPES.items()})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ECOCOMFORT2_CLIENT_SCHEMA),
    cv.has_at_least_one_key(*SENSOR_TYPES),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await register_ecocomfort2_child(var, config)

    if conf := config.get(CONF_CONNECTED):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(var.set_connected_sensor(sens))

    if conf := config.get(CONF_BOOST):
        sens = await binary_sensor.new_binary_sensor(conf)
        cg.add(var.set_boost_sensor(sens))
