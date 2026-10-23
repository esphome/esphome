import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import CONF_DUCO_ID, DUCO_COMPONENT_SCHEMA, duco_ns

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

DucoSerial = duco_ns.class_("DucoSerial", cg.PollingComponent, text_sensor.TextSensor)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(DucoSerial)
    .extend(cv.polling_component_schema("300s"))
    .extend(DUCO_COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_DUCO_ID])
