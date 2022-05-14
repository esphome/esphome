import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TX_POWER, CONF_TYPE, CONF_UUID
from esphome.components import esp32_ble_tracker

DEPENDENCIES = ["esp32_ble_tracker"]

esp32_ble_beacon_ns = cg.esphome_ns.namespace("esp32_ble_beacon")
ESP32BLEBeacon = esp32_ble_beacon_ns.class_("ESP32BLEBeacon", cg.Component)

CONF_MAJOR = "major"
CONF_MINOR = "minor"
MEASURED_POWER = "measured_power"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESP32BLEBeacon),
            cv.Required(CONF_TYPE): cv.one_of("IBEACON", upper=True),
            cv.Required(CONF_UUID): cv.uuid,
            cv.Optional(CONF_MAJOR, default=10167): cv.uint16_t,
            cv.Optional(CONF_MINOR, default=61958): cv.uint16_t,
            cv.Optional(CONF_TX_POWER, default=7): cv.int_range(0, 7),
            cv.Optional(MEASURED_POWER, default=-59): cv.int_range(-100, 9),
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    uuid = config[CONF_UUID].hex
    uuid_arr = [cg.RawExpression(f"0x{uuid[i:i + 2]}") for i in range(0, len(uuid), 2)]
    var = cg.new_Pvariable(config[CONF_ID], uuid_arr)
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_client(var, config)

    cg.add(var.set_major(config[CONF_MAJOR]))
    cg.add(var.set_minor(config[CONF_MINOR]))
    cg.add(var.set_txpower_level(config[CONF_TX_POWER]))
    cg.add(var.set_measured_power(config[MEASURED_POWER]))
