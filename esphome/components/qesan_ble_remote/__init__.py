import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble_tracker
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_MAC_ADDRESS

CODEOWNERS = ["@Mat931"]
MULTI_CONF = True
DEPENDENCIES = ["esp32_ble_tracker"]

qesan_ble_remote_ns = cg.esphome_ns.namespace("qesan_ble_remote")
QesanListener = qesan_ble_remote_ns.class_(
    "QesanListener", esp32_ble_tracker.ESPBTDeviceListener, cg.PollingComponent
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(QesanListener),
            cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_ADDRESS): cv.hex_uint16_t,
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("1s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await esp32_ble_tracker.register_ble_device(var, config)

    if (mac_address := config.get(CONF_MAC_ADDRESS)) is not None:
        cg.add(var.set_mac_address(mac_address.as_hex))
    if (remote_address := config.get(CONF_ADDRESS)) is not None:
        cg.add(var.set_remote_address(remote_address))
