import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv

from . import CONF_LIS3DH_ID, LIS3DHComponent

CONF_TAP = "tap"
CONF_DOUBLE_TAP = "double_tap"
CONF_ACTIVITY = "activity"
CONF_FREEFALL = "freefall"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LIS3DH_ID): cv.use_id(LIS3DHComponent),
        cv.Optional(CONF_TAP): binary_sensor.binary_sensor_schema(
            icon="mdi:gesture-tap"
        ),
        cv.Optional(CONF_DOUBLE_TAP): binary_sensor.binary_sensor_schema(
            icon="mdi:gesture-double-tap"
        ),
        cv.Optional(CONF_ACTIVITY): binary_sensor.binary_sensor_schema(
            icon="mdi:motion-sensor"
        ),
        cv.Optional(CONF_FREEFALL): binary_sensor.binary_sensor_schema(
            icon="mdi:arrow-down"
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LIS3DH_ID])

    if tap_conf := config.get(CONF_TAP):
        cg.add(
            hub.set_tap_binary_sensor(await binary_sensor.new_binary_sensor(tap_conf))
        )
    if dt_conf := config.get(CONF_DOUBLE_TAP):
        cg.add(
            hub.set_double_tap_binary_sensor(
                await binary_sensor.new_binary_sensor(dt_conf)
            )
        )
    if act_conf := config.get(CONF_ACTIVITY):
        cg.add(
            hub.set_activity_binary_sensor(
                await binary_sensor.new_binary_sensor(act_conf)
            )
        )
    if ff_conf := config.get(CONF_FREEFALL):
        cg.add(
            hub.set_freefall_binary_sensor(
                await binary_sensor.new_binary_sensor(ff_conf)
            )
        )
