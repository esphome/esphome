from esphome.components import text_sensor
from esphome.components.const import CONF_SERIAL_NUMBER
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_VERSION
from esphome.types import ConfigType

from .. import (
    CONF_DUCO_ID,
    DUCO_COMPONENT_SCHEMA,
    DucoDevice,
    duco_ns,
    register_duco_device,
)

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

DucoTextSensor = duco_ns.class_("DucoTextSensor", DucoDevice, text_sensor.TextSensor)
DucoTextReading = duco_ns.enum("DucoTextReading", is_class=True)

CONF_ERROR = "error"
CONF_UPTIME = "uptime"
CONF_NODE_TYPE = "node_type"

# key: (what is read, lowest address, update interval); address 0 of a serial number is the box itself
TEXT_SENSORS = {
    CONF_SERIAL_NUMBER: (DucoTextReading.DUCO_TEXT_READING_SERIAL_NUMBER, 0, "300s"),
    CONF_VERSION: (DucoTextReading.DUCO_TEXT_READING_VERSION, 1, "300s"),
    CONF_ERROR: (DucoTextReading.DUCO_TEXT_READING_ERROR, 1, "60s"),
    CONF_UPTIME: (DucoTextReading.DUCO_TEXT_READING_UPTIME, 1, "60s"),
    CONF_NODE_TYPE: (DucoTextReading.DUCO_TEXT_READING_NODE_TYPE, 1, "300s"),
}


def _schema(min_address: int, update_interval: str) -> cv.Schema:
    return (
        text_sensor.text_sensor_schema(DucoTextSensor)
        .extend(
            {
                cv.Optional(CONF_ADDRESS, default=min_address): cv.int_range(
                    min=min_address, max=0x44
                ),
            }
        )
        .extend(cv.polling_component_schema(update_interval))
    )


_SCHEMAS = {
    key: _schema(min_address, update_interval)
    for key, (_, min_address, update_interval) in TEXT_SENSORS.items()
}

CONFIG_SCHEMA = cv.Any(
    _SCHEMAS[CONF_SERIAL_NUMBER].extend(DUCO_COMPONENT_SCHEMA),
    cv.All(
        DUCO_COMPONENT_SCHEMA.extend(
            {
                cv.Optional(key): cv.ensure_list(schema)
                for key, schema in _SCHEMAS.items()
            }
        ),
        cv.has_at_least_one_key(*TEXT_SENSORS),
    ),
)


async def to_code(config: ConfigType) -> None:
    if CONF_ADDRESS in config:
        var = await text_sensor.new_text_sensor(
            config,
            TEXT_SENSORS[CONF_SERIAL_NUMBER][0],
            config[CONF_ADDRESS],
        )
        await register_duco_device(var, config, config[CONF_DUCO_ID])
        return

    for key, (reading, _, _) in TEXT_SENSORS.items():
        for entity_config in config.get(key, []):
            var = await text_sensor.new_text_sensor(
                entity_config, reading, entity_config[CONF_ADDRESS]
            )
            await register_duco_device(var, entity_config, config[CONF_DUCO_ID])
