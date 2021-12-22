import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ENTITY_CATEGORY,
    CONF_ID,
    CONF_DEVICE_CLASS,
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

status_ns = cg.esphome_ns.namespace("status")
StatusBinarySensor = status_ns.class_(
    "StatusBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(StatusBinarySensor),
        cv.Optional(
            CONF_DEVICE_CLASS, default=DEVICE_CLASS_CONNECTIVITY
        ): binary_sensor.device_class,
        cv.Optional(
            CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_DIAGNOSTIC
        ): cv.entity_category,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)
