from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import DUCO_COMPONENT_SCHEMA, DucoDevice, duco_ns, register_duco_device

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

DucoSerial = duco_ns.class_("DucoSerial", DucoDevice, text_sensor.TextSensor)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(DucoSerial)
    .extend(cv.polling_component_schema("300s"))
    .extend(DUCO_COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = await text_sensor.new_text_sensor(config)
    await register_duco_device(var, config)
