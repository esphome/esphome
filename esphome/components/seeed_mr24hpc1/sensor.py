import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_SPEED,
    UNIT_METER,
)
from . import CONF_MR24HPC1_ID, mr24hpc1Component

AUTO_LOAD = ["mr24hpc1"]

CONF_CUSTOMPRESENCEOFDETECTION = "custom_presence_of_detection"
CONF_MOVEMENTSIGNS = "movement_signs"
CONF_CUSTOMMOTIONDISTANCE = "custom_motion_distance"
CONF_CUSTOMSPATIALSTATICVALUE = "custom_spatial_static_value"
CONF_CUSTOMSPATIALMOTIONVALUE = "custom_spatial_motion_value"
CONF_CUSTOMMOTIONSPEED =  "custom_motion_speed"

CONF_CUSTOMMODENUM = "custom_mode_num"


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MR24HPC1_ID): cv.use_id(mr24hpc1Component),
        cv.Optional(CONF_CUSTOMPRESENCEOFDETECTION): sensor.sensor_schema(
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,                   # Specify the number of decimal places
            icon="mdi:signal-distance-variant",
        ),
        cv.Optional(CONF_MOVEMENTSIGNS): sensor.sensor_schema(
            icon="mdi:human-greeting-variant",
        ),
        cv.Optional(CONF_CUSTOMMOTIONDISTANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            icon="mdi:signal-distance-variant",
        ),
        cv.Optional(CONF_CUSTOMSPATIALSTATICVALUE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_ENERGY,
            icon="mdi:counter",
        ),
        cv.Optional(CONF_CUSTOMSPATIALMOTIONVALUE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_ENERGY,
            icon="mdi:counter",
        ),
        cv.Optional(CONF_CUSTOMMOTIONSPEED): sensor.sensor_schema(
            unit_of_measurement="m/s",
            device_class=DEVICE_CLASS_SPEED,
            accuracy_decimals=2,
            icon="mdi:run-fast",
        ),
        cv.Optional(CONF_CUSTOMMODENUM): sensor.sensor_schema(
            icon="mdi:counter",
        ),
    }
)


async def to_code(config):
    mr24hpc1_component = await cg.get_variable(config[CONF_MR24HPC1_ID])
    if custompresenceofdetection_config := config.get(CONF_CUSTOMPRESENCEOFDETECTION):
        sens = await sensor.new_sensor(custompresenceofdetection_config)
        cg.add(mr24hpc1_component.set_custom_presence_of_detection_sensor(sens))
    if movementsigns_config := config.get(CONF_MOVEMENTSIGNS):
        sens = await sensor.new_sensor(movementsigns_config)
        cg.add(mr24hpc1_component.set_movementSigns_sensor(sens))
    if custommotiondistance_config := config.get(CONF_CUSTOMMOTIONDISTANCE):
        sens = await sensor.new_sensor(custommotiondistance_config)
        cg.add(mr24hpc1_component.set_custom_motion_distance_sensor(sens))
    if customspatialstaticvalue_config := config.get(CONF_CUSTOMSPATIALSTATICVALUE):
        sens = await sensor.new_sensor(customspatialstaticvalue_config)
        cg.add(mr24hpc1_component.set_custom_spatial_static_value_sensor(sens))
    if customspatialmotionvalue_config := config.get(CONF_CUSTOMSPATIALMOTIONVALUE):
        sens = await sensor.new_sensor(customspatialmotionvalue_config)
        cg.add(mr24hpc1_component.set_custom_spatial_motion_value_sensor(sens))
    if custommotionspeed_config := config.get(CONF_CUSTOMMOTIONSPEED):
        sens = await sensor.new_sensor(custommotionspeed_config)
        cg.add(mr24hpc1_component.set_custom_motion_speed_sensor(sens))
    if custommodenum_config := config.get(CONF_CUSTOMMODENUM):
        sens = await sensor.new_sensor(custommodenum_config)
        cg.add(mr24hpc1_component.set_custom_mode_num_sensor(sens))
