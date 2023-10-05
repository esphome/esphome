import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_OCCUPANCY,
    ICON_MOTION_SENSOR,
)
from . import CONF_MR24HPB1_ID, MR24HPB1Component

DEPENDENCIES = ["mr24hpb1"]

# Occupancy binary sensor
CONF_OCCUPANCY = "occupancy"

# Movment binary sensor
CONF_MOVEMENT = "movement"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MR24HPB1_ID): cv.use_id(MR24HPB1Component),
        cv.Optional(CONF_OCCUPANCY): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_OCCUPANCY,
        ),
        cv.Optional(CONF_MOVEMENT): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_MOTION,
            icon=ICON_MOTION_SENSOR,
        ),
    }
)


async def to_code(config):
    var = await cg.get_variable(config[CONF_MR24HPB1_ID])

    if CONF_OCCUPANCY in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_OCCUPANCY])
        cg.add(var.set_occupancy_sensor(sens))

    if CONF_MOVEMENT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_MOVEMENT])
        cg.add(var.set_movement_sensor(sens))
