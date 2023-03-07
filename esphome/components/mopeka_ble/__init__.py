import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32_ble_tracker
from esphome.const import CONF_ID

CODEOWNERS = ["@spbrogan", "@Fabian-Schmidt"]
DEPENDENCIES = ["esp32_ble_tracker"]

CONF_SHOW_SENSORS_WITHOUT_SYNC = "show_sensors_without_sync"

mopeka_ble_ns = cg.esphome_ns.namespace("mopeka_ble")
MopekaListener = mopeka_ble_ns.class_(
    "MopekaListener", esp32_ble_tracker.ESPBTDeviceListener
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MopekaListener),
        cv.Optional(CONF_SHOW_SENSORS_WITHOUT_SYNC, default=False): cv.boolean,
    }
).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if CONF_SHOW_SENSORS_WITHOUT_SYNC in config:
        cg.add(
            var.set_show_sensors_without_sync(config[CONF_SHOW_SENSORS_WITHOUT_SYNC])
        )
    await esp32_ble_tracker.register_ble_device(var, config)
