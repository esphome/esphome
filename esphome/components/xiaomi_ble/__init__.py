import esphome.codegen as cg
from esphome.components import ble_device_base
import esphome.config_validation as cv
from esphome.const import CONF_BINDKEY, CONF_ID, CONF_MAC_ADDRESS
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

AUTO_LOAD = ["ble_device_base"]
DOMAIN = "xiaomi_ble"

xiaomi_ble_ns = cg.esphome_ns.namespace("xiaomi_ble")
XiaomiListener = xiaomi_ble_ns.class_(
    "XiaomiListener", ble_device_base.ESPBTDeviceListener
)
XiaomiDevice = xiaomi_ble_ns.class_("XiaomiDevice", ble_device_base.ESPBTDeviceListener)
XiaomiEncryptedDevice = xiaomi_ble_ns.class_("XiaomiEncryptedDevice", XiaomiDevice)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("xiaomi_ble"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiListener),
        }
    ).extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await ble_device_base.register_ble_device(var, config)


def setup_xiaomi_device(var: MockObj, config: ConfigType) -> None:
    """Set the address, and the bindkey when configured, of a XiaomiDevice."""
    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add_optional(var.set_bindkey, config.get(CONF_BINDKEY))
