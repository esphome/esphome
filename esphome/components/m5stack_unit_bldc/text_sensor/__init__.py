import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_STATUS
from esphome.types import ConfigType

from .. import CONF_M5STACK_UNIT_BLDC_ID, M5StackUnitBldc

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_M5STACK_UNIT_BLDC_ID): cv.use_id(M5StackUnitBldc),
    cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_M5STACK_UNIT_BLDC_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_STATUS, hub.set_status_text_sensor)
