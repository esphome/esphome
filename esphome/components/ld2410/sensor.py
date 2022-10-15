import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_ENERGY,
    UNIT_CENTIMETER,
    UNIT_PERCENT,
)
from . import CONF_LD2410_ID, LD2410Component

DEPENDENCIES = ["ld2410"]
CONF_MOVINGTARGETDISTANCE = "moving_distance"
CONF_STILLTARGETDISTANCE = "still_distance"
CONF_MOVINGTARGETENERGY = "moving_energy"
CONF_STILLTARGETENERGY = "still_energy"
CONF_DETECTIONDISTANCE = "detection_distance"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_MOVINGTARGETDISTANCE): sensor.sensor_schema(
        device_class=DEVICE_CLASS_DISTANCE, unit_of_measurement=UNIT_CENTIMETER
    ),
    cv.Optional(CONF_STILLTARGETDISTANCE): sensor.sensor_schema(
        device_class=DEVICE_CLASS_DISTANCE, unit_of_measurement=UNIT_CENTIMETER
    ),
    cv.Optional(CONF_MOVINGTARGETENERGY): sensor.sensor_schema(
        device_class=DEVICE_CLASS_ENERGY, unit_of_measurement=UNIT_PERCENT
    ),
    cv.Optional(CONF_STILLTARGETENERGY): sensor.sensor_schema(
        device_class=DEVICE_CLASS_ENERGY, unit_of_measurement=UNIT_PERCENT
    ),
    cv.Optional(CONF_DETECTIONDISTANCE): sensor.sensor_schema(
        device_class=DEVICE_CLASS_DISTANCE, unit_of_measurement=UNIT_CENTIMETER
    )
    # cv.Optional(CONF_HASMOVINGTARGET): binary_sensor.binary_sensor_schema(
    #     device_class=DEVICE_CLASS_MOTION
    # ),
    # cv.Optional(CONF_HASSTILLTARGET): binary_sensor.binary_sensor_schema(
    #     device_class=DEVICE_CLASS_PRESENCE
    # ),
}


async def to_code(config):
    ld2410_component = await cg.get_variable(config[CONF_LD2410_ID])
    if CONF_MOVINGTARGETDISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_MOVINGTARGETDISTANCE])
        cg.add(ld2410_component.setmovingdistancesensor(sens))
    if CONF_STILLTARGETDISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_STILLTARGETDISTANCE])
        cg.add(ld2410_component.setstilldistancesensor(sens))
    if CONF_MOVINGTARGETENERGY in config:
        sens = await sensor.new_sensor(config[CONF_MOVINGTARGETENERGY])
        cg.add(ld2410_component.setmovingenergysensor(sens))
    if CONF_STILLTARGETENERGY in config:
        sens = await sensor.new_sensor(config[CONF_STILLTARGETENERGY])
        cg.add(ld2410_component.setstillenergysensor(sens))
    if CONF_DETECTIONDISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_DETECTIONDISTANCE])
        cg.add(ld2410_component.setdetectiondistancesensor(sens))
