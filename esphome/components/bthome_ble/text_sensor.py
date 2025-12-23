import esphome.codegen as cg
from esphome.components import esp32_ble_tracker, text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, DEVICE_CLASS_FIRMWARE, ENTITY_CATEGORY_DIAGNOSTIC

from . import BTHomeBLE, bthome_ble_base_schema, setup_bthome_ble

CONF_FIRMWARE_VERSION = "firmware_version"

CODEOWNERS = ["@esphome/core"]

DEPENDENCIES = ["esp32_ble_tracker"]
MULTI_CONF = True

CONFIG_SCHEMA = bthome_ble_base_schema(
    {
        cv.Optional(CONF_FIRMWARE_VERSION): text_sensor.text_sensor_schema(
            device_class=DEVICE_CLASS_FIRMWARE,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        )
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await setup_bthome_ble(var, config)

    if CONF_FIRMWARE_VERSION in config:
        sens = await text_sensor.new_text_sensor(config[CONF_FIRMWARE_VERSION])
        cg.add(var.set_firmware(sens))
