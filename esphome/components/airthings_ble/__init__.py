import esphome.codegen as cg
from esphome.components import ble_device_base
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["ble_device_base"]
CODEOWNERS = ["@jeromelaban"]

airthings_ble_ns = cg.esphome_ns.namespace("airthings_ble")
AirthingsListener = airthings_ble_ns.class_(
    "AirthingsListener", ble_device_base.ESPBTDeviceListener
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("airthings_ble"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AirthingsListener),
        }
    ).extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ble_device_base.register_ble_device(var, config)
