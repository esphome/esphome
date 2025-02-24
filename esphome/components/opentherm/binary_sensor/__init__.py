from typing import Any

from esphome.components import binary_sensor
import esphome.config_validation as cv

from .. import const, generate, schema, validate

DEPENDENCIES = [const.OPENTHERM]
COMPONENT_TYPE = const.BINARY_SENSOR


def get_entity_validation_schema(entity: schema.BinarySensorSchema) -> cv.Schema:
    return binary_sensor.binary_sensor_schema(
        device_class=(
            entity.device_class or binary_sensor._UNDEF  # pylint: disable=protected-access
        ),
        icon=(entity.icon or binary_sensor._UNDEF),  # pylint: disable=protected-access
    )


CONFIG_SCHEMA = validate.create_component_schema(
    schema.BINARY_SENSORS, get_entity_validation_schema
)


async def to_code(config: dict[str, Any]) -> None:
    await generate.component_to_code(
        COMPONENT_TYPE,
        schema.BINARY_SENSORS,
        binary_sensor.BinarySensor,
        generate.create_only_conf(binary_sensor.new_binary_sensor),
        config,
    )
