import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_MAC_ADDRESS
from esphome.components.esp32_bt_classic import ESP32BtClassic
from esphome.components.esp32_bt_classic.const import (
    CONF_ESP32_BTCLASSIC_ID,
    CONF_NUM_SCANS,
)

DEPENDENCIES = ["esp32_bt_classic"]

bt_classic_presence_ns = cg.esphome_ns.namespace("bt_classic_presence")
BTClassicPresenceDevice = bt_classic_presence_ns.class_(
    "BTClassicPresenceDevice",
    binary_sensor.BinarySensor,
    cg.PollingComponent,
)


CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(BTClassicPresenceDevice)
    .extend(
        {
            cv.GenerateID(CONF_ESP32_BTCLASSIC_ID): cv.use_id(ESP32BtClassic),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_NUM_SCANS, default=1): cv.uint8_t,
        }
    )
    .extend(cv.polling_component_schema("5min")),
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_ESP32_BTCLASSIC_ID])
    var = await binary_sensor.new_binary_sensor(
        config, paren, config[CONF_MAC_ADDRESS].as_hex, config[CONF_NUM_SCANS]
    )
    await cg.register_component(var, config)
