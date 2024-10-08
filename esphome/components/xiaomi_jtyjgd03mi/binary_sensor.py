import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_SMOKE,
    CONF_TIMEOUT,
    DEVICE_CLASS_SMOKE,
    CONF_ID,
    CONF_BUTTON,
)
from esphome.core import TimePeriod

from . import XiaomiJTYJQD03MI

DEPENDENCIES = ["xiaomi_jtyjgd03mi"]


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(XiaomiJTYJQD03MI),
        cv.Optional(CONF_SMOKE): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_SMOKE
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


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ID])

    if CONF_SMOKE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_SMOKE])
        cg.add(parent.set_smoke(sens))

    if CONF_BUTTON in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_BUTTON])
        cg.add(parent.set_button(sens))
        cg.add(parent.set_button_timeout(config[CONF_BUTTON][CONF_TIMEOUT]))
