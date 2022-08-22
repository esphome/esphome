from esphome import automation
from esphome.components import esp32_ble_tracker
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_MAC_ADDRESS, CONF_NAME, CONF_TRIGGER_ID

DEPENDENCIES = ["esp32", "esp32_ble_tracker", "api"]
# CODEOWNERS = ["@jesserockz"]

CONF_DEVICES = "devices"
CONF_ON_BLE_ADVERTISEMENT = "on_ble_advertisement"

bluetooth_proxy_ns = cg.esphome_ns.namespace("bluetooth_proxy")

BluetoothProxy = bluetooth_proxy_ns.class_("BluetoothProxy", cg.Component)

BLEAdvertisementTrigger = bluetooth_proxy_ns.class_(
    "BLEAdvertisementTrigger",
    automation.Trigger.template(esp32_ble_tracker.ESPBTDeviceConstRef, cg.std_string),
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BluetoothProxy),
        cv.Optional(CONF_DEVICES, default=[]): cv.ensure_list(
            cv.Schema(
                {
                    cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
                    cv.Required(CONF_NAME): cv.string,
                }
            )
        ),
        cv.Optional(
            CONF_ON_BLE_ADVERTISEMENT, default=[]
        ): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEAdvertisementTrigger),
            }
        ),
    }
).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    for device in config[CONF_DEVICES]:
        cg.add(var.add_device(device[CONF_MAC_ADDRESS].as_hex, device[CONF_NAME]))

    for conf in config.get(CONF_ON_BLE_ADVERTISEMENT):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [
                (esp32_ble_tracker.ESPBTDeviceConstRef, "device"),
                (cg.std_string, "packet"),
            ],
            conf,
        )

    await esp32_ble_tracker.register_ble_device(var, config)

    cg.add_define("USE_BLUETOOTH_PROXY")
