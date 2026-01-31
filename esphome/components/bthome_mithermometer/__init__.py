import esphome.codegen as cg
from esphome.components import esp32_ble_tracker
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MAC_ADDRESS

CODEOWNERS = ["@nagyrobi"]
DEPENDENCIES = ["esp32_ble_tracker"]

BLE_DEVICE_SCHEMA = esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA

bthome_mithermometer_ns = cg.esphome_ns.namespace("bthome_mithermometer")
BTHomeMiThermometer = bthome_mithermometer_ns.class_(
    "BTHomeMiThermometer", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)


def bthome_mithermometer_base_schema(extra_schema=None):
    if extra_schema is None:
        extra_schema = {}
    return (
        cv.Schema(
            {
                cv.GenerateID(CONF_ID): cv.declare_id(BTHomeMiThermometer),
                cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            }
        )
        .extend(BLE_DEVICE_SCHEMA)
        .extend(cv.COMPONENT_SCHEMA)
        .extend(extra_schema)
    )


async def setup_bthome_mithermometer(var, config):
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)
    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
