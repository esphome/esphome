import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_BATTERY_LEVEL, CONF_ID, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from . import XiaomiRTCGQ02LM

DEPENDENCIES = ["xiaomi_rtcgq02lm"]


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(XiaomiRTCGQ02LM),
        cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
            kind=sensor.BATTERY,
            accuracy_decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ID])

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_BATTERY_LEVEL, hub.set_battery_level_sensor)
