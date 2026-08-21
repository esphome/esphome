import esphome.codegen as cg
from esphome.components import ble_device_base
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["ble_device_base"]

xiaomi_ble_ns = cg.esphome_ns.namespace("xiaomi_ble")
XiaomiListener = xiaomi_ble_ns.class_(
    "XiaomiListener", ble_device_base.ESPBTDeviceListener
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("xiaomi_ble"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiListener),
        }
    ).extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ble_device_base.register_ble_device(var, config)
