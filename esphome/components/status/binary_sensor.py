import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

DEPENDENCIES = ["network"]

status_ns = cg.esphome_ns.namespace("status")
StatusBinarySensor = status_ns.class_(
    "StatusBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    StatusBinarySensor,
    device_class=DEVICE_CLASS_CONNECTIVITY,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
