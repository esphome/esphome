import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from . import CONF_LIS3DH_ID, LIS3DHComponent

CONF_ORIENTATION_XY = "orientation_xy"
CONF_ORIENTATION_Z = "orientation_z"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LIS3DH_ID): cv.use_id(LIS3DHComponent),
        cv.Optional(CONF_ORIENTATION_XY): text_sensor.text_sensor_schema(
            icon="mdi:axis-x-y-arrow-right"
        ),
        cv.Optional(CONF_ORIENTATION_Z): text_sensor.text_sensor_schema(
            icon="mdi:axis-z-arrow"
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LIS3DH_ID])

    if xy_conf := config.get(CONF_ORIENTATION_XY):
        cg.add(
            hub.set_orientation_xy_text_sensor(
                await text_sensor.new_text_sensor(xy_conf)
            )
        )
    if z_conf := config.get(CONF_ORIENTATION_Z):
        cg.add(
            hub.set_orientation_z_text_sensor(await text_sensor.new_text_sensor(z_conf))
        )
