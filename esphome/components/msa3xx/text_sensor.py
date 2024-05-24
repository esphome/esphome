import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_NAME,
)
from . import MSA3xxComponent, CONF_MSA3XX_ID

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["msa3xx"]

CONF_ORIENTATION_XY = "orientation_xy"
CONF_ORIENTATION_Z = "orientation_z"

ICON_SCREEN_ROTATION = "mdi:screen-rotation"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_MSA3XX_ID): cv.use_id(MSA3xxComponent),
            cv.Optional(CONF_ORIENTATION_XY): cv.maybe_simple_value(
                text_sensor.text_sensor_schema(icon=ICON_SCREEN_ROTATION),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_ORIENTATION_Z): cv.maybe_simple_value(
                text_sensor.text_sensor_schema(icon=ICON_SCREEN_ROTATION),
                key=CONF_NAME,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        var = await text_sensor.new_text_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_text_sensor")(var))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_MSA3XX_ID])

    SENSORS = [CONF_ORIENTATION_XY, CONF_ORIENTATION_Z]

    for key in SENSORS:
        await setup_conf(config, key, hub)
