import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_NAME

from . import CONF_LIS2DH12_ID, LIS2DH12_SENSOR_SCHEMA

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["lis2dh12_base"]

CONF_ORIENTATION = "orientation"
ICON_SCREEN_ROTATION = "mdi:screen-rotation"

CONFIG_SCHEMA = LIS2DH12_SENSOR_SCHEMA.extend(
    {
        cv.Optional(CONF_ORIENTATION): cv.maybe_simple_value(
            text_sensor.text_sensor_schema(icon=ICON_SCREEN_ROTATION),
            key=CONF_NAME,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LIS2DH12_ID])

    if sensor_config := config.get(CONF_ORIENTATION):
        var = await text_sensor.new_text_sensor(sensor_config)
        cg.add(hub.set_orientation_text_sensor(var))
