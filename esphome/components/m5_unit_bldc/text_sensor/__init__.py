import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_STATUS

from .. import CONF_M5_UNIT_BLDC_ID, M5UnitBldc

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_M5_UNIT_BLDC_ID): cv.use_id(M5UnitBldc),
    cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(),
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_M5_UNIT_BLDC_ID])

    if status_config := config.get(CONF_STATUS):
        sens = await text_sensor.new_text_sensor(status_config)
        cg.add(parent.set_status_text_sensor(sens))
