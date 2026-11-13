import esphome.codegen as cg
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

DucoSerial = duco_ns.class_("DucoSerial", DucoDevice, text_sensor.TextSensor)
DucoVersion = duco_ns.class_("DucoVersion", DucoDevice, text_sensor.TextSensor)
DucoError = duco_ns.class_("DucoError", DucoDevice, text_sensor.TextSensor)
DucoUptime = duco_ns.class_("DucoUptime", DucoDevice, text_sensor.TextSensor)
DucoNodeType = duco_ns.class_("DucoNodeType", DucoDevice, text_sensor.TextSensor)

CONF_ERROR = "error"
CONF_UPTIME = "uptime"
CONF_NODE_TYPE = "node_type"


def _schema(cls: cg.MockObjClass, min_address: int, update_interval: str) -> cv.Schema:
    return (
        text_sensor.text_sensor_schema(cls)
        .extend(
            {
                cv.Optional(CONF_ADDRESS, default=min_address): cv.int_range(
                    min=min_address, max=0x44
                ),
            }
        )
        .extend(cv.polling_component_schema(update_interval))
    )


# key: schema of one entry; address 0 of a serial number is the box itself
TEXT_SENSORS = {
    CONF_SERIAL_NUMBER: _schema(DucoSerial, 0, "300s"),
    CONF_VERSION: _schema(DucoVersion, 1, "300s"),
    CONF_ERROR: _schema(DucoError, 1, "60s"),
    CONF_UPTIME: _schema(DucoUptime, 1, "60s"),
    CONF_NODE_TYPE: _schema(DucoNodeType, 1, "300s"),
}

CONFIG_SCHEMA = cv.Any(
    TEXT_SENSORS[CONF_SERIAL_NUMBER].extend(DUCO_COMPONENT_SCHEMA),
    cv.All(
        DUCO_COMPONENT_SCHEMA.extend(
            {
                cv.Optional(key): cv.ensure_list(schema)
                for key, schema in TEXT_SENSORS.items()
            }
        ),
        cv.has_at_least_one_key(*TEXT_SENSORS),
    ),
)


async def to_code(config: ConfigType) -> None:
    if CONF_ADDRESS in config:
        var = await text_sensor.new_text_sensor(config, config[CONF_ADDRESS])
        await register_duco_device(var, config, config[CONF_DUCO_ID])
        return

    for key in TEXT_SENSORS:
        for entity_config in config.get(key, []):
            var = await text_sensor.new_text_sensor(
                entity_config, entity_config[CONF_ADDRESS]
            )
            await register_duco_device(var, entity_config, config[CONF_DUCO_ID])
