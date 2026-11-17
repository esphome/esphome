import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DISPLAY,
    DEVICE_CLASS_LIGHT,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_RUNNING,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.types import ConfigType

from .climate import CONF_MIDEA_ID, AirConditioner

CODEOWNERS = ["@dudanov"]

CONF_DEFROST = "defrost"
CONF_FILTER_ALERT = "filter_alert"
CONF_FOLLOW_ME = "follow_me"
CONF_WATER_PUMP = "water_pump"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MIDEA_ID): cv.use_id(AirConditioner),
        # Group 5
        cv.Optional(CONF_DEFROST): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_RUNNING,
            icon="mdi:snowflake-melt",
        ),
        # Group 2
        cv.Optional(CONF_WATER_PUMP): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_RUNNING,
            icon="mdi:water-pump",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        # 0xC0 status
        cv.Optional(CONF_FILTER_ALERT): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            icon="mdi:air-filter",
        ),
        cv.Optional(CONF_DISPLAY): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_LIGHT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_FOLLOW_ME): binary_sensor.binary_sensor_schema(
            icon="mdi:thermometer-check",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MIDEA_ID])
    sensors = binary_sensor.sub_binary_sensors(config)
    await sensors(CONF_DEFROST, hub.set_defrost_binary_sensor)
    await sensors(CONF_WATER_PUMP, hub.set_water_pump_binary_sensor)
    await sensors(CONF_FILTER_ALERT, hub.set_filter_alert_binary_sensor)
    await sensors(CONF_DISPLAY, hub.set_display_binary_sensor)
    await sensors(CONF_FOLLOW_ME, hub.set_follow_me_binary_sensor)
