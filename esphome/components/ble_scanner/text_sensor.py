import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor, esp32_ble_tracker
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32_ble_tracker"]

ble_scanner_ns = cg.esphome_ns.namespace("ble_scanner")
BLEScanner = ble_scanner_ns.class_(
    "BLEScanner",
    text_sensor.TextSensor,
    cg.Component,
    esp32_ble_tracker.ESPBTDeviceListener,
)

CONFIG_SCHEMA = cv.All(
    text_sensor.TEXT_SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(BLEScanner),
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)
    await text_sensor.register_text_sensor(var, config)
