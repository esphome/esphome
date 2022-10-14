from esphome.components import esp32_ble_tracker, esp32_ble_client
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ACTIVE, CONF_ID

AUTO_LOAD = ["esp32_ble_client", "esp32_ble_tracker"]
DEPENDENCIES = ["api", "esp32"]
CODEOWNERS = ["@jesserockz"]


bluetooth_proxy_ns = cg.esphome_ns.namespace("bluetooth_proxy")

BluetoothProxy = bluetooth_proxy_ns.class_(
    "BluetoothProxy", esp32_ble_client.BLEClientBase
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BluetoothProxy),
        cv.Optional(CONF_ACTIVE, default=False): cv.boolean,
    }
).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_active(config[CONF_ACTIVE]))

    await esp32_ble_tracker.register_client(var, config)

    cg.add_define("USE_BLUETOOTH_PROXY")
