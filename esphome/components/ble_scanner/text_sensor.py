import esphome.codegen as cg
from esphome.components import ble_device_base, text_sensor
import esphome.config_validation as cv

AUTO_LOAD = ["ble_device_base"]

ble_scanner_ns = cg.esphome_ns.namespace("ble_scanner")
BLEScanner = ble_scanner_ns.class_(
    "BLEScanner",
    text_sensor.TextSensor,
    cg.Component,
    ble_device_base.ESPBTDeviceListener,
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("ble_scanner"),
    text_sensor.text_sensor_schema(BLEScanner)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    await ble_device_base.register_ble_device(var, config)
