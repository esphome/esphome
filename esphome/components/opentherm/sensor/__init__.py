from typing import Any

from esphome.components import sensor
import esphome.config_validation as cv

from .. import const, generate, schema, validate

DEPENDENCIES = [const.OPENTHERM]
COMPONENT_TYPE = const.SENSOR

MSG_DATA_TYPES = {
    "u8_lb",
    "u8_hb",
    "s8_lb",
    "s8_hb",
    "u8_lb_60",
    "u8_hb_60",
    "u16",
    "s16",
    "f88",
}


def get_entity_validation_schema(entity: schema.SensorSchema) -> cv.Schema:
    return sensor.sensor_schema(
        unit_of_measurement=entity.unit_of_measurement or sensor._UNDEF,  # pylint: disable=protected-access
        accuracy_decimals=entity.accuracy_decimals,
        device_class=entity.device_class or sensor._UNDEF,  # pylint: disable=protected-access
        icon=entity.icon or sensor._UNDEF,  # pylint: disable=protected-access
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
        sensor.Sensor,
        generate.create_only_conf(sensor.new_sensor),
        config,
    )
