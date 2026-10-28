import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_SPEED,
    ICON_SIGNAL,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
    UNIT_METER_PER_SECOND,
)
from esphome.types import ConfigType

from . import (
    CONF_DFROBOT_SEN0609_ID,
    MODE_SPEED_AND_DISTANCE,
    DFRobotSEN0609Component,
    final_validate_mode,
)

DEPENDENCIES = ["dfrobot_sen0609"]

CONF_TARGET_DISTANCE = "target_distance"
CONF_TARGET_ENERGY = "target_energy"
CONF_TARGET_SPEED = "target_speed"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_DFROBOT_SEN0609_ID): cv.use_id(DFRobotSEN0609Component),
    cv.Optional(CONF_TARGET_DISTANCE): sensor.sensor_schema(
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_DISTANCE,
        state_class=STATE_CLASS_MEASUREMENT,
        unit_of_measurement=UNIT_METER,
    ),
    cv.Optional(CONF_TARGET_SPEED): sensor.sensor_schema(
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_SPEED,
        state_class=STATE_CLASS_MEASUREMENT,
        unit_of_measurement=UNIT_METER_PER_SECOND,
    ),
    cv.Optional(CONF_TARGET_ENERGY): sensor.sensor_schema(
        accuracy_decimals=0,
        icon=ICON_SIGNAL,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}

FINAL_VALIDATE_SCHEMA = final_validate_mode(
    {
        CONF_TARGET_DISTANCE: MODE_SPEED_AND_DISTANCE,
        CONF_TARGET_ENERGY: MODE_SPEED_AND_DISTANCE,
        CONF_TARGET_SPEED: MODE_SPEED_AND_DISTANCE,
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DFROBOT_SEN0609_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TARGET_DISTANCE, hub.set_target_distance_sensor)
    await sensors(CONF_TARGET_SPEED, hub.set_target_speed_sensor)
    await sensors(CONF_TARGET_ENERGY, hub.set_target_energy_sensor)
