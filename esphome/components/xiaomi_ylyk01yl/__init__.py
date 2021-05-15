import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, esp32_ble_tracker
from esphome import automation
from esphome.const import (
    CONF_MAC_ADDRESS,
    UNIT_EMPTY,
    ICON_EMPTY,
    DEVICE_CLASS_EMPTY,
    CONF_ID,
    CONF_TRIGGER_ID,
)

DEPENDENCIES = ["esp32_ble_tracker"]
AUTO_LOAD = ["xiaomi_ble", "sensor"]
MULTI_CONF = True

CONF_LAST_BUTTON_PRESSED = "last_button_pressed"
CONF_ON_BUTTON_ON = "on_button_on"
CONF_ON_BUTTON_OFF = "on_button_off"
CONF_ON_BUTTON_SUN = "on_button_sun"
CONF_ON_BUTTON_M = "on_button_m"
CONF_ON_BUTTON_PLUS = "on_button_plus"
CONF_ON_BUTTON_MINUS = "on_button_minus"

ON_PRESS_ACTIONS = [
    CONF_ON_BUTTON_ON,
    CONF_ON_BUTTON_OFF,
    CONF_ON_BUTTON_SUN,
    CONF_ON_BUTTON_M,
    CONF_ON_BUTTON_PLUS,
    CONF_ON_BUTTON_MINUS,
]

xiaomi_ylyk01yl_ns = cg.esphome_ns.namespace("xiaomi_ylyk01yl")
XiaomiYLYK01YL = xiaomi_ylyk01yl_ns.class_(
    "XiaomiYLYK01YL", esp32_ble_tracker.ESPBTDeviceListener, cg.Component
)

OnButtonOnTrigger = xiaomi_ylyk01yl_ns.class_(
    "OnButtonOnTrigger", automation.Trigger.template()
)
OnButtonOffTrigger = xiaomi_ylyk01yl_ns.class_(
    "OnButtonOffTrigger", automation.Trigger.template()
)
OnButtonSunTrigger = xiaomi_ylyk01yl_ns.class_(
    "OnButtonSunTrigger", automation.Trigger.template()
)
OnButtonMTrigger = xiaomi_ylyk01yl_ns.class_(
    "OnButtonMTrigger", automation.Trigger.template()
)
OnButtonPlusTrigger = xiaomi_ylyk01yl_ns.class_(
    "OnButtonPlusTrigger", automation.Trigger.template()
)
OnButtonMinusTrigger = xiaomi_ylyk01yl_ns.class_(
    "OnButtonMinusTrigger", automation.Trigger.template()
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(XiaomiYLYK01YL),
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_LAST_BUTTON_PRESSED): sensor.sensor_schema(
                UNIT_EMPTY, ICON_EMPTY, 1, DEVICE_CLASS_EMPTY
            ),
            cv.Optional(CONF_ON_BUTTON_ON): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        OnButtonOnTrigger),
                }
            ),
            cv.Optional(CONF_ON_BUTTON_OFF): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        OnButtonOffTrigger),
                }
            ),
            cv.Optional(CONF_ON_BUTTON_SUN): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        OnButtonSunTrigger),
                }
            ),
            cv.Optional(CONF_ON_BUTTON_M): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        OnButtonMTrigger),
                }
            ),
            cv.Optional(CONF_ON_BUTTON_PLUS): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        OnButtonPlusTrigger),
                }
            ),
            cv.Optional(CONF_ON_BUTTON_MINUS): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        OnButtonMinusTrigger),
                }
            ),
        }
    )
    .extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield esp32_ble_tracker.register_ble_device(var, config)

    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))

    if CONF_LAST_BUTTON_PRESSED in config:
        sens = yield sensor.new_sensor(config[CONF_LAST_BUTTON_PRESSED])
        cg.add(var.set_keycode(sens))

    for action in ON_PRESS_ACTIONS:
        for conf in config.get(action, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            yield automation.build_automation(trigger, [], conf)
