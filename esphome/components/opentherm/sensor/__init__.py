from typing import Any

import esphome.config_validation as cv
from esphome.components import sensor
from .. import const, schema, validate, generate

DEPENDENCIES = [const.OPENTHERM]
COMPONENT_TYPE = const.SENSOR


def get_entity_validation_schema(entity: schema.SensorSchema) -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement=entity.unit_of_measurement
        or sensor._UNDEF,  # pylint: disable=protected-access
        accuracy_decimals=entity.accuracy_decimals,
        device_class=entity.device_class
        or sensor._UNDEF,  # pylint: disable=protected-access
        icon=entity.icon or sensor._UNDEF,  # pylint: disable=protected-access
        state_class=entity.state_class,
    )


CONFIG_SCHEMA = validate.create_component_schema(
    schema.SENSORS, get_entity_validation_schema
)


async def to_code(config: dict[str, Any]) -> None:
    await generate.component_to_code(
        COMPONENT_TYPE,
        schema.SENSORS,
        sensor.Sensor,
        generate.create_only_conf(sensor.new_sensor),
        config,
    )
