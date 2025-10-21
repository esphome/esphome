from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32_ble_tracker
import esphome.config_validation as cv
from esphome.const import CONF_TRIGGER_ID

CODEOWNERS = ["@OttoWinter"]
DEPENDENCIES = ["esp32_ble_tracker"]

exposure_notifications_ns = cg.esphome_ns.namespace("exposure_notifications")
ExposureNotification = exposure_notifications_ns.struct("ExposureNotification")
ExposureNotificationTrigger = exposure_notifications_ns.class_(
    "ExposureNotificationTrigger",
    esp32_ble_tracker.ESPBTDeviceListener,
    automation.Trigger.template(ExposureNotification),
)

CONF_ON_EXPOSURE_NOTIFICATION = "on_exposure_notification"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ON_EXPOSURE_NOTIFICATION): automation.validate_automation(
            cv.Schema(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        ExposureNotificationTrigger
                    ),
                }
            ).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
        ),
    }
)


async def to_code(config):
    for conf in config.get(CONF_ON_EXPOSURE_NOTIFICATION, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        await automation.build_automation(trigger, [(ExposureNotification, "x")], conf)
        await esp32_ble_tracker.register_ble_device(trigger, conf)
