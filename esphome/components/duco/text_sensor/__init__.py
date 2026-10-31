import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_TYPE
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

TYPE_SERIAL = "serial"
TYPE_VERSION = "version"
TYPE_ERROR = "error"
TYPE_UPTIME = "uptime"

SERIAL_SCHEMA = (
    text_sensor.text_sensor_schema(DucoSerial)
    .extend(
        {
            cv.Optional(CONF_ADDRESS, default=0): cv.int_range(min=0, max=0x45),
        }
    )
    .extend(cv.polling_component_schema("300s"))
    .extend(DUCO_COMPONENT_SCHEMA)
)

VERSION_SCHEMA = (
    text_sensor.text_sensor_schema(DucoVersion)
    .extend(
        {
            cv.Required(CONF_ADDRESS): cv.int_range(min=1, max=0x45),
        }
    )
    .extend(cv.polling_component_schema("300s"))
    .extend(DUCO_COMPONENT_SCHEMA)
)

ERROR_SCHEMA = (
    text_sensor.text_sensor_schema(DucoError)
    .extend(
        {
            cv.Required(CONF_ADDRESS): cv.int_range(min=1, max=0x45),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(DUCO_COMPONENT_SCHEMA)
)

UPTIME_SCHEMA = (
    text_sensor.text_sensor_schema(DucoUptime)
    .extend(
        {
            cv.Required(CONF_ADDRESS): cv.int_range(min=1, max=0x45),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(DUCO_COMPONENT_SCHEMA)
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        TYPE_SERIAL: SERIAL_SCHEMA,
        TYPE_VERSION: VERSION_SCHEMA,
        TYPE_ERROR: ERROR_SCHEMA,
        TYPE_UPTIME: UPTIME_SCHEMA,
    },
    default_type=TYPE_SERIAL,
)


async def to_code(config: ConfigType) -> None:
    var = await text_sensor.new_text_sensor(config)
    await register_duco_device(var, config, config[CONF_DUCO_ID])
    cg.add(var.set_address(config[CONF_ADDRESS]))
