import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_BATTERY_CHARGING,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from esphome.types import ConfigType

from . import CONF_NPM1300_ID, NPM1300_CHILD_SCHEMA

DEPENDENCIES = ["npm1300"]

CONF_VBUS_PRESENT = "vbus_present"
CONF_CHARGING = "charging"
CONF_CHARGE_COMPLETE = "charge_complete"
CONF_BATTERY_DETECTED = "battery_detected"
CONF_CHARGER_ERROR = "charger_error"

CONFIG_SCHEMA = NPM1300_CHILD_SCHEMA.extend(
    {
        cv.Optional(CONF_VBUS_PRESENT): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_POWER,
        ),
        cv.Optional(CONF_CHARGING): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_BATTERY_CHARGING,
        ),
        cv.Optional(CONF_CHARGE_COMPLETE): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_BATTERY_DETECTED): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_CONNECTIVITY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_CHARGER_ERROR): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_NPM1300_ID])
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_VBUS_PRESENT, hub.set_vbus_present_binary_sensor)
    await binary_sensors(CONF_CHARGING, hub.set_charging_binary_sensor)
    await binary_sensors(CONF_CHARGE_COMPLETE, hub.set_charge_complete_binary_sensor)
    await binary_sensors(CONF_BATTERY_DETECTED, hub.set_battery_detected_binary_sensor)
    await binary_sensors(CONF_CHARGER_ERROR, hub.set_charger_error_binary_sensor)
