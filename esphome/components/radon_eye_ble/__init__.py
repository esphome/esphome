import esphome.codegen as cg
from esphome.components import ble_device_base
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

AUTO_LOAD = ["ble_device_base"]
CODEOWNERS = ["@jeffeb3"]

radon_eye_ble_ns = cg.esphome_ns.namespace("radon_eye_ble")
RadonEyeListener = radon_eye_ble_ns.class_(
    "RadonEyeListener", ble_device_base.ESPBTDeviceListener
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("radon_eye_ble"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RadonEyeListener),
        }
    ).extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await ble_device_base.register_ble_device(var, config)
