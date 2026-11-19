import esphome.codegen as cg
from esphome.components import ble_device_base, xiaomi_ble
import esphome.config_validation as cv
from esphome.const import CONF_BINDKEY, CONF_MAC_ADDRESS
from esphome.types import ConfigType

AUTO_LOAD = ["ble_device_base", "xiaomi_ble"]
CODEOWNERS = ["@jesserockz"]
DOMAIN = "xiaomi_rtcgq02lm"
MULTI_CONF = True

xiaomi_rtcgq02lm_ns = cg.esphome_ns.namespace("xiaomi_rtcgq02lm")
XiaomiRTCGQ02LM = xiaomi_rtcgq02lm_ns.class_(
    "XiaomiRTCGQ02LM", xiaomi_ble.XiaomiEncryptedDevice, cg.Component
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("xiaomi_rtcgq02lm"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiRTCGQ02LM),
            cv.Required(CONF_BINDKEY): cv.bind_key,
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    var = await ble_device_base.new_ble_device_component(config)

    xiaomi_ble.setup_xiaomi_device(var, config)
