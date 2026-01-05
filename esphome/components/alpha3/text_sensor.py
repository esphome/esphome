import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv

from . import CONF_ALPHA3_ID, Alpha3

CONF_PUMP_MODE = "pump_mode"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ALPHA3_ID): cv.use_id(Alpha3),
        cv.Optional(CONF_PUMP_MODE): text_sensor.text_sensor_schema(),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ALPHA3_ID])

    if mode_config := config.get(CONF_PUMP_MODE):
        sens = await text_sensor.new_text_sensor(mode_config)
        cg.add(parent.set_mode_text_sensor(sens))
