import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_MODE
from esphome.types import ConfigType

from .water_heater import CONF_HAIER_WATER_HEATER_ID, HonWaterHeater

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HAIER_WATER_HEATER_ID): cv.use_id(HonWaterHeater),
        cv.Optional(CONF_MODE): text_sensor.text_sensor_schema(),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_HAIER_WATER_HEATER_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_MODE, hub.set_mode_text_sensor)
