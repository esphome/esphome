import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS
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

CONF_SERIAL = "serial"
CONF_VERSION = "version"
CONF_ERROR = "error"
CONF_UPTIME = "uptime"
CONF_NODE_TYPE = "node_type"

SERIAL_ENTITY_SCHEMA = (
    text_sensor.text_sensor_schema(DucoSerial)
    .extend(
        {
            cv.Optional(CONF_ADDRESS, default=0): cv.int_range(min=0, max=0x45),
        }
    )
    .extend(cv.polling_component_schema("300s"))
)

SERIAL_SCHEMA = SERIAL_ENTITY_SCHEMA.extend(DUCO_COMPONENT_SCHEMA)

VERSION_SCHEMA = (
    text_sensor.text_sensor_schema(DucoVersion)
    .extend(
        {
            cv.Optional(CONF_ADDRESS, default=1): cv.int_range(min=1, max=0x45),
        }
    )
    .extend(cv.polling_component_schema("300s"))
)

ERROR_SCHEMA = (
    text_sensor.text_sensor_schema(DucoError)
    .extend(
        {
            cv.Optional(CONF_ADDRESS, default=1): cv.int_range(min=1, max=0x45),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)

UPTIME_SCHEMA = (
    text_sensor.text_sensor_schema(DucoUptime)
    .extend(
        {
            cv.Optional(CONF_ADDRESS, default=1): cv.int_range(min=1, max=0x45),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)

NODE_TYPE_SCHEMA = (
    text_sensor.text_sensor_schema(DucoNodeType)
    .extend(
        {
            cv.Optional(CONF_ADDRESS, default=1): cv.int_range(min=1, max=0x45),
        }
    )
    .extend(cv.polling_component_schema("300s"))
)

CONFIG_SCHEMA = cv.Any(
    SERIAL_SCHEMA,
    DUCO_COMPONENT_SCHEMA.extend(
        {
            cv.Optional(CONF_SERIAL): cv.ensure_list(SERIAL_ENTITY_SCHEMA),
            cv.Optional(CONF_VERSION): cv.ensure_list(VERSION_SCHEMA),
            cv.Optional(CONF_ERROR): cv.ensure_list(ERROR_SCHEMA),
            cv.Optional(CONF_UPTIME): cv.ensure_list(UPTIME_SCHEMA),
            cv.Optional(CONF_NODE_TYPE): cv.ensure_list(NODE_TYPE_SCHEMA),
        }
    ),
)


async def to_code(config: ConfigType) -> None:
    if all(key not in config for key in (CONF_SERIAL, CONF_VERSION, CONF_ERROR, CONF_UPTIME, CONF_NODE_TYPE)):
        var = await text_sensor.new_text_sensor(config)
        await register_duco_device(var, config, config[CONF_DUCO_ID])
        cg.add(var.set_address(config[CONF_ADDRESS]))
        return

    for key in (CONF_SERIAL, CONF_VERSION, CONF_ERROR, CONF_UPTIME, CONF_NODE_TYPE):
        for entity_config in config.get(key, []):
            var = await text_sensor.new_text_sensor(entity_config)
            await register_duco_device(var, entity_config, config[CONF_DUCO_ID])
            cg.add(var.set_address(entity_config[CONF_ADDRESS]))
