import esphome.codegen as cg
from esphome.components import esp32_ble_tracker
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MAC_ADDRESS

CODEOWNERS = ["@nagyrobi"]
DEPENDENCIES = ["esp32_ble_tracker"]
AUTO_LOAD = ["esp32_ble_tracker"]

DEFAULT_ESP32_BLE_ID = "esp32_ble_tracker_esp32bletracker_id"

BLE_DEVICE_SCHEMA = cv.Schema(
    {
        cv.Optional(
            esp32_ble_tracker.CONF_ESP32_BLE_ID, default=DEFAULT_ESP32_BLE_ID
        ): cv.use_id(esp32_ble_tracker.ESP32BLETracker),
    }
)

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
    tracker_config = dict(config)
    tracker_config.setdefault(esp32_ble_tracker.CONF_ESP32_BLE_ID, DEFAULT_ESP32_BLE_ID)
    await esp32_ble_tracker.register_ble_device(var, tracker_config)
    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
