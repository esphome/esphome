import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCELERATION_X,
    CONF_ACCELERATION_Y,
    CONF_ACCELERATION_Z,
    ICON_ACCELERATION_X,
    ICON_ACCELERATION_Y,
    ICON_ACCELERATION_Z,
    STATE_CLASS_MEASUREMENT,
)
from esphome.types import ConfigType

from . import CONF_WII_NUNCHUK_ID, WII_NUNCHUK_CHILD_SCHEMA

DEPENDENCIES = ["wii_nunchuk"]

CONF_JOYSTICK_X = "joystick_x"
CONF_JOYSTICK_Y = "joystick_y"


def _axis_schema(icon: str) -> cv.Schema:
    return sensor.sensor_schema(
        icon=icon, accuracy_decimals=0, state_class=STATE_CLASS_MEASUREMENT
    )


CONFIG_SCHEMA = WII_NUNCHUK_CHILD_SCHEMA.extend(
    {
        cv.Optional(CONF_JOYSTICK_X): _axis_schema("mdi:gamepad-square"),
        cv.Optional(CONF_JOYSTICK_Y): _axis_schema("mdi:gamepad-square"),
        cv.Optional(CONF_ACCELERATION_X): _axis_schema(ICON_ACCELERATION_X),
        cv.Optional(CONF_ACCELERATION_Y): _axis_schema(ICON_ACCELERATION_Y),
        cv.Optional(CONF_ACCELERATION_Z): _axis_schema(ICON_ACCELERATION_Z),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_WII_NUNCHUK_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_JOYSTICK_X, hub.set_joystick_x_sensor)
    await sensors(CONF_JOYSTICK_Y, hub.set_joystick_y_sensor)
    await sensors(CONF_ACCELERATION_X, hub.set_acceleration_x_sensor)
    await sensors(CONF_ACCELERATION_Y, hub.set_acceleration_y_sensor)
    await sensors(CONF_ACCELERATION_Z, hub.set_acceleration_z_sensor)
