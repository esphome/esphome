from typing import Any

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv

from .. import const, generate, schema, validate

DEPENDENCIES = [const.OPENTHERM]
COMPONENT_TYPE = const.SENSOR

# All the types except the flags, which should be binary sensors
MSG_DATA_TYPES = {x for x in generate.TYPE_LIST if not x.startswith("flag8_")}

OpenthermSensor = generate.opentherm_ns.class_(
    "OpenthermSensor", sensor.Sensor, generate.MessageProcessor
)


async def new_opentherm_sensor(
    config: dict[str, Any], key: str, hub: cg.MockObj
) -> cg.MockObj:
    return await sensor.new_sensor(
        config, generate.accessor_template(schema.SENSORS[key])
    )


def get_entity_validation_schema(entity: schema.SensorSchema) -> cv.Schema:
    return sensor.sensor_schema(
        OpenthermSensor,
        unit_of_measurement=entity.unit_of_measurement or cv.UNDEFINED,  # pylint: disable=protected-access
        accuracy_decimals=entity.accuracy_decimals,
        device_class=entity.device_class or cv.UNDEFINED,  # pylint: disable=protected-access
        icon=entity.icon or cv.UNDEFINED,  # pylint: disable=protected-access
        state_class=entity.state_class,
    ).extend(
        {
            cv.Optional(const.CONF_DATA_TYPE): cv.one_of(*MSG_DATA_TYPES),
        }
    )


CONFIG_SCHEMA = validate.create_component_schema(
    schema.SENSORS, get_entity_validation_schema
)


async def to_code(config: dict[str, Any]) -> None:
    await generate.component_to_code(
        COMPONENT_TYPE,
        schema.SENSORS,
        OpenthermSensor,
        new_opentherm_sensor,
        config,
    )
