import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    UNIT_METER,
)
from . import CONF_MR24HPC1_ID, mr24hpc1Component

AUTO_LOAD = ["mr24hpc1"]

CONF_CUSTOMPRESENCEOFDETECTION = "custompresenceofdetection"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MR24HPC1_ID): cv.use_id(mr24hpc1Component),
        cv.Optional(CONF_CUSTOMPRESENCEOFDETECTION): sensor.sensor_schema(
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_METER,
            icon="mdi:signal-distance-variant",
        ),
    }
)


async def to_code(config):
    mr24hpc1_component = await cg.get_variable(config[CONF_MR24HPC1_ID])
    if custompresenceofdetection_config := config.get(CONF_CUSTOMPRESENCEOFDETECTION):
        sens = await sensor.new_sensor(custompresenceofdetection_config)
        cg.add(mr24hpc1_component.set_custom_presence_of_detection_sensor(sens))
