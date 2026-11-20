import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import CONF_WII_NUNCHUK_ID, WII_NUNCHUK_CHILD_SCHEMA

DEPENDENCIES = ["wii_nunchuk"]

CONF_C_BUTTON = "c_button"
CONF_Z_BUTTON = "z_button"

CONFIG_SCHEMA = WII_NUNCHUK_CHILD_SCHEMA.extend(
    {
        cv.Optional(CONF_C_BUTTON): binary_sensor.binary_sensor_schema(
            icon="mdi:alpha-c-box"
        ),
        cv.Optional(CONF_Z_BUTTON): binary_sensor.binary_sensor_schema(
            icon="mdi:alpha-z-box"
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_WII_NUNCHUK_ID])
    sensors = binary_sensor.sub_binary_sensors(config)
    await sensors(CONF_C_BUTTON, hub.set_c_button_binary_sensor)
    await sensors(CONF_Z_BUTTON, hub.set_z_button_binary_sensor)
