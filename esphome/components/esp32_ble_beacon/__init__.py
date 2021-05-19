import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE, CONF_UUID, ESP_PLATFORM_ESP32

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]
CONFLICTS_WITH = ["esp32_ble_tracker"]

esp32_ble_beacon_ns = cg.esphome_ns.namespace("esp32_ble_beacon")
ESP32BLEBeacon = esp32_ble_beacon_ns.class_("ESP32BLEBeacon", cg.Component)

CONF_MAJOR = "major"
CONF_MINOR = "minor"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32BLEBeacon),
        cv.Required(CONF_TYPE): cv.one_of("IBEACON", upper=True),
        cv.Required(CONF_UUID): cv.uuid,
        cv.Optional(CONF_MAJOR, default=10167): cv.uint16_t,
        cv.Optional(CONF_MINOR, default=61958): cv.uint16_t,
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    uuid = config[CONF_UUID].hex
    uuid_arr = [
        cg.RawExpression("0x{}".format(uuid[i : i + 2])) for i in range(0, len(uuid), 2)
    ]
    var = cg.new_Pvariable(config[CONF_ID], uuid_arr)
    yield cg.register_component(var, config)
    cg.add(var.set_major(config[CONF_MAJOR]))
    cg.add(var.set_minor(config[CONF_MINOR]))
