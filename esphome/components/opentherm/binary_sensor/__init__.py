from typing import Any

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv

from .. import const, generate, schema, validate

DEPENDENCIES = [const.OPENTHERM]
COMPONENT_TYPE = const.BINARY_SENSOR


OpenthermBinarySensor = generate.opentherm_ns.class_(
    "OpenthermBinarySensor", binary_sensor.BinarySensor, generate.MessageProcessor
)


async def new_opentherm_binary_sensor(
    config: dict[str, Any], key: str, hub: cg.MockObj
) -> cg.MockObj:
    return await binary_sensor.new_binary_sensor(
        config, generate.accessor_template(schema.BINARY_SENSORS[key])
    )


def get_entity_validation_schema(entity: schema.BinarySensorSchema) -> cv.Schema:
    return binary_sensor.binary_sensor_schema(
        OpenthermBinarySensor,
        device_class=(entity.device_class or cv.UNDEFINED),
        icon=(entity.icon or cv.UNDEFINED),
    )


CONFIG_SCHEMA = validate.create_component_schema(
    schema.BINARY_SENSORS, get_entity_validation_schema
)


async def to_code(config: dict[str, Any]) -> None:
    await generate.component_to_code(
        COMPONENT_TYPE,
        schema.BINARY_SENSORS,
        OpenthermBinarySensor,
        new_opentherm_binary_sensor,
        config,
    )
