import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_NAME

from . import CONF_MSA3XX_ID, MSA_SENSOR_SCHEMA

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["msa3xx"]

CONF_ORIENTATION_XY = "orientation_xy"
CONF_ORIENTATION_Z = "orientation_z"
ICON_SCREEN_ROTATION = "mdi:screen-rotation"

ORIENTATION_SENSORS = (CONF_ORIENTATION_XY, CONF_ORIENTATION_Z)

CONFIG_SCHEMA = MSA_SENSOR_SCHEMA.extend(
    {
        cv.Optional(sensor): cv.maybe_simple_value(
            text_sensor.text_sensor_schema(icon=ICON_SCREEN_ROTATION),
            key=CONF_NAME,
        )
        for sensor in ORIENTATION_SENSORS
    }
)


async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        var = await text_sensor.new_text_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_text_sensor")(var))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_MSA3XX_ID])

    for key in ORIENTATION_SENSORS:
        await setup_conf(config, key, hub)
