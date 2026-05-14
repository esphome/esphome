import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_BATTERY_CHARGING,
    DEVICE_CLASS_PRESENCE,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from .. import CONF_AXP2101_ID, AXP2101Component

DEPENDENCIES = ["axp2101"]

CONF_BATTERY_PRESENT = "battery_present"
CONF_BATTERY_CHARGING = "battery_charging"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AXP2101_ID): cv.use_id(AXP2101Component),
        cv.Optional(CONF_BATTERY_CHARGING): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_BATTERY_CHARGING
        ),
        cv.Optional(CONF_BATTERY_PRESENT): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PRESENCE,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_AXP2101_ID])

    if CONF_BATTERY_PRESENT in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_BATTERY_PRESENT])
        cg.add(parent.set_battery_present_sensor(sens))

    if CONF_BATTERY_CHARGING in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_BATTERY_CHARGING])
        cg.add(parent.set_battery_charging_sensor(sens))
