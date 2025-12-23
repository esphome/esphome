import esphome.codegen as cg
from esphome.components import esp32_ble_tracker
import esphome.config_validation as cv
from esphome.const import CONF_MAC_ADDRESS

DEPENDENCIES = ["esp32_ble_tracker"]
AUTO_LOAD = ["esp32_ble_tracker"]

bthome_ble_ns = cg.esphome_ns.namespace("bthome_ble")
BTHomeBLE = bthome_ble_ns.class_(
    "BTHomeBLE", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)


def bthome_ble_base_schema(extra_schema=None):
    if extra_schema is None:
        extra_schema = {}
    return (
        cv.Schema({cv.GenerateID(): cv.declare_id(BTHomeBLE), cv.Required(CONF_MAC_ADDRESS): cv.mac_address})
        .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
        .extend(cv.COMPONENT_SCHEMA)
        .extend(extra_schema)
    )


async def setup_bthome_ble(var, config):
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)
    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
