import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUTTON,
    CONF_ID,
    CONF_LIGHT,
    CONF_MOTION,
    CONF_TIMEOUT,
    DEVICE_CLASS_LIGHT,
    DEVICE_CLASS_MOTION,
)
from esphome.core import TimePeriod
from esphome.types import ConfigType

from . import XiaomiRTCGQ02LM

DEPENDENCIES = ["xiaomi_rtcgq02lm"]


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(XiaomiRTCGQ02LM),
        cv.Optional(CONF_MOTION): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_MOTION
        ).extend(
            {
                cv.Optional(CONF_TIMEOUT, default="5s"): cv.All(
                    cv.positive_time_period_milliseconds,
                    cv.Range(max=TimePeriod(milliseconds=65535)),
                ),
            }
        ),
        cv.Optional(CONF_LIGHT): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_LIGHT
        ),
        cv.Optional(CONF_BUTTON): binary_sensor.binary_sensor_schema().extend(
            {
                cv.Optional(CONF_TIMEOUT, default="200ms"): cv.All(
                    cv.positive_time_period_milliseconds,
                    cv.Range(max=TimePeriod(milliseconds=65535)),
                ),
            }
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_ID])

    if await binary_sensor.new_sub_binary_sensor(
        config, CONF_MOTION, parent.set_motion
    ):
        cg.add(parent.set_motion_timeout(config[CONF_MOTION][CONF_TIMEOUT]))
    await binary_sensor.new_sub_binary_sensor(config, CONF_LIGHT, parent.set_light)
    if await binary_sensor.new_sub_binary_sensor(
        config, CONF_BUTTON, parent.set_button
    ):
        cg.add(parent.set_button_timeout(config[CONF_BUTTON][CONF_TIMEOUT]))
