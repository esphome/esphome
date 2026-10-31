from esphome.components import text_sensor
import esphome.codegen as cg
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

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(DucoSerial)
    .extend(
        {
            cv.Optional(CONF_ADDRESS, default=0): cv.int_range(min=0, max=0x45),
        }
    )
    .extend(cv.polling_component_schema("300s"))
    .extend(DUCO_COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = await text_sensor.new_text_sensor(config)
    await register_duco_device(var, config, config[CONF_DUCO_ID])
    cg.add(var.set_address(config[CONF_ADDRESS]))
