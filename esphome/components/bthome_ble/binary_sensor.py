import esphome.codegen as cg
from esphome.components import binary_sensor, esp32_ble_tracker
import esphome.config_validation as cv
from esphome.const import CONF_ID, DEVICE_CLASS_BATTERY, ENTITY_CATEGORY_DIAGNOSTIC

from . import BTHomeBLE, bthome_ble_base_schema, setup_bthome_ble

CONF_BATTERY = "battery"

CODEOWNERS = ["@esphome/core"]

DEPENDENCIES = ["esp32_ble_tracker"]

CONFIG_SCHEMA = bthome_ble_base_schema(
    {
        cv.Optional(CONF_BATTERY): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_BATTERY,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        )
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await setup_bthome_ble(var, config)

    if CONF_BATTERY in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_BATTERY])
        cg.add(var.set_battery_low(sens))
