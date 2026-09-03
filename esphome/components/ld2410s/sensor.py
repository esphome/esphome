import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_DISTANCE, UNIT_CENTIMETER, UNIT_PERCENT

from . import CONF_LD2410S_ID, LD2410S

CONF_CALIBRATION_PROGRESS = "calibration_progress"
CONF_TARGET_DISTANCE = "target_distance"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410S_ID): cv.use_id(LD2410S),
    cv.Optional(CONF_CALIBRATION_PROGRESS): sensor.sensor_schema(
        unit_of_measurement=UNIT_PERCENT,
        icon="mdi:percent",
    ),
    cv.Optional(CONF_TARGET_DISTANCE): sensor.sensor_schema(
        device_class=DEVICE_CLASS_DISTANCE,
        unit_of_measurement=UNIT_CENTIMETER,
        icon="mdi:arrow-left-right",
    ),
}


async def to_code(config):
    ld2410s = await cg.get_variable(config[CONF_LD2410S_ID])
    if calibration_progress_config := config.get(CONF_CALIBRATION_PROGRESS):
        sens = await sensor.new_sensor(calibration_progress_config)
        cg.add(ld2410s.set_calibration_progress_sensor(sens))
    if target_distance_config := config.get(CONF_TARGET_DISTANCE):
        sens = await sensor.new_sensor(target_distance_config)
        cg.add(ld2410s.set_distance_sensor(sens))
