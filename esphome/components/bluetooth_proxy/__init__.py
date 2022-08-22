from esphome.components import esp32_ble_tracker
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32", "esp32_ble_tracker"]
CODEOWNERS = ["@jesserockz"]


bluetooth_proxy_ns = cg.esphome_ns.namespace("bluetooth_proxy")

BluetoothProxy = bluetooth_proxy_ns.class_("BluetoothProxy", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BluetoothProxy),
    }
).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await esp32_ble_tracker.register_ble_device(var, config)

    cg.add_define("USE_BLUETOOTH_PROXY")
