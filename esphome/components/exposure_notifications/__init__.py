from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components import ble_device_base
import esphome.config_validation as cv
from esphome.const import CONF_TRIGGER_ID
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

CODEOWNERS = ["@OttoWinter"]
AUTO_LOAD = ["ble_device_base"]

exposure_notifications_ns = cg.esphome_ns.namespace("exposure_notifications")
ExposureNotification = exposure_notifications_ns.struct("ExposureNotification")
ExposureNotificationTrigger = exposure_notifications_ns.class_(
    "ExposureNotificationTrigger",
    ble_device_base.ESPBTDeviceListener,
    automation.Trigger.template(ExposureNotification),
)

CONF_ON_EXPOSURE_NOTIFICATION = "on_exposure_notification"

_RENAME_HUB_ID = ble_device_base.rename_legacy_hub_id("exposure_notifications")

_VALIDATE_AUTOMATION = automation.validate_automation(
    cv.Schema(
        {
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ExposureNotificationTrigger),
        }
        # The trigger is the BLE listener, so the hub id lives on it.
    ).extend(ble_device_base.BLE_DEVICE_SCHEMA)
)


# validate_automation() needs a dict-based schema, so the rename cannot go
# inside it and has to run on the option value first. That value may also be a
# list of automations or malformed, and rename_legacy_hub_id() is dict-only, so
# map over lists and let validate_automation() report anything else.
# schema_extractor keeps the key typed as a trigger in the generated editor
# schema; build_language_schema.py recurses into cv.All but not into a plain
# function.
@schema_extractor("automation")
def _validate_on_exposure_notification(value: Any) -> list[ConfigType]:
    if value is SCHEMA_EXTRACT:
        return _VALIDATE_AUTOMATION(value)
    if isinstance(value, dict):
        value = _RENAME_HUB_ID(value)
    elif isinstance(value, list):
        value = [_RENAME_HUB_ID(v) if isinstance(v, dict) else v for v in value]
    return _VALIDATE_AUTOMATION(value)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ON_EXPOSURE_NOTIFICATION): _validate_on_exposure_notification,
    }
)


async def to_code(config):
    for conf in config.get(CONF_ON_EXPOSURE_NOTIFICATION, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        await automation.build_automation(trigger, [(ExposureNotification, "x")], conf)
        await ble_device_base.register_ble_device(trigger, conf)
