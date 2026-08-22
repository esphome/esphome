import esphome.codegen as cg
from esphome.components import ble_device_base
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@spbrogan", "@Fabian-Schmidt"]
AUTO_LOAD = ["ble_device_base"]

CONF_SHOW_SENSORS_WITHOUT_SYNC = "show_sensors_without_sync"

mopeka_ble_ns = cg.esphome_ns.namespace("mopeka_ble")
MopekaListener = mopeka_ble_ns.class_(
    "MopekaListener", ble_device_base.ESPBTDeviceListener
)

CONFIG_SCHEMA = cv.All(
    ble_device_base.rename_legacy_hub_id("mopeka_ble"),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MopekaListener),
            cv.Optional(CONF_SHOW_SENSORS_WITHOUT_SYNC, default=False): cv.boolean,
        }
    ).extend(ble_device_base.BLE_DEVICE_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if CONF_SHOW_SENSORS_WITHOUT_SYNC in config:
        cg.add(
            var.set_show_sensors_without_sync(config[CONF_SHOW_SENSORS_WITHOUT_SYNC])
        )
    await ble_device_base.register_ble_device(var, config)
