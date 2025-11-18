import esphome.codegen as cg
from esphome.components import esp32_ble_tracker
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_BINDKEY

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["esp32_ble_tracker"]

bthome_ble_ns = cg.esphome_ns.namespace("bthome_ble")
BTHomeListener = bthome_ble_ns.class_(
    "BTHomeListener", esp32_ble_tracker.ESPBTDeviceListener
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BTHomeListener),
        cv.Optional(CONF_BINDKEY): cv.bind_key,
    }
).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await esp32_ble_tracker.register_ble_device(var, config)

    if bindkey := config.get(CONF_BINDKEY):
        cg.add(var.set_bindkey(bindkey))
