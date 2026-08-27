import esphome.codegen as cg
from esphome.components import ble_device_base
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

AUTO_LOAD = ["ble_device_base"]

ruuvi_ble_ns = cg.esphome_ns.namespace("ruuvi_ble")
RuuviListener = ruuvi_ble_ns.class_(
    "RuuviListener", ble_device_base.ESPBTDeviceListener
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("ruuvi_ble"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RuuviListener),
        }
    ).extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await ble_device_base.register_ble_device(var, config)
