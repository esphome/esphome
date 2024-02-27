import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_MOTION,
    ICON_MOTION_SENSOR,
)

from . import CONF_AT581X_ID, AT581XComponent

DEPENDENCIES = ["at581x"]

CONF_MOTION_DETECTED = "motion_detected"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_AT581X_ID): cv.use_id(AT581XComponent),
    cv.Optional(CONF_MOTION_DETECTED): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_MOTION,
        icon=ICON_MOTION_SENSOR,
    ),
}


async def to_code(config):
    at581x_component = await cg.get_variable(config[CONF_AT581X_ID])
    if has_target_config := config.get(CONF_MOTION_DETECTED):
        sens = await binary_sensor.new_binary_sensor(has_target_config)
        cg.add(at581x_component.set_motion_binary_sensor(sens))
