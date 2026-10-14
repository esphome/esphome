import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_NAME, ICON_SCREEN_ROTATION
from esphome.types import ConfigType

from . import CONF_LIS2DW12_ID, LIS2DW12_SENSOR_SCHEMA

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["lis2dw12"]

CONF_ORIENTATION = "orientation"

CONFIG_SCHEMA = LIS2DW12_SENSOR_SCHEMA.extend(
    {
        cv.Optional(CONF_ORIENTATION): cv.maybe_simple_value(
            text_sensor.text_sensor_schema(icon=ICON_SCREEN_ROTATION),
            key=CONF_NAME,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LIS2DW12_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_ORIENTATION, hub.set_orientation_text_sensor)
