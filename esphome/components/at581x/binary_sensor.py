import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_MOTION,
    ICON_MOTION_SENSOR,
)

from . import CONF_AT581X_ID, AT581XComponent

DEPENDENCIES = ["at581x"]

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_MOTION,
        icon=ICON_MOTION_SENSOR,
    ).extend(cv.Schema(
        {
            cv.GenerateID(CONF_AT581X_ID): cv.use_id(AT581XComponent),
        }
    )
)

async def to_code(config):
    at581x_component = await cg.get_variable(config[CONF_AT581X_ID])
    if has_target_config := config.get(CONF_HAS_TARGET):
        sens = await binary_sensor.new_binary_sensor(has_target_config)
        cg.add(at581x_component.set_motion_binary_sensor(sens))
