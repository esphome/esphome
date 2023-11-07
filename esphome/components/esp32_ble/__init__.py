import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID
from esphome.core import CORE
from esphome.components.esp32 import add_idf_sdkconfig_option, get_esp32_variant, const

DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@jesserockz", "@Rapsssito"]
CONFLICTS_WITH = ["esp32_ble_beacon"]

CONF_BLE_ID = "ble_id"
CONF_IO_CAPABILITY = "io_capability"
CONF_ENABLE_ON_BOOT = "enable_on_boot"

NO_BLUETOOTH_VARIANTS = [const.VARIANT_ESP32S2]

esp32_ble_ns = cg.esphome_ns.namespace("esp32_ble")
ESP32BLE = esp32_ble_ns.class_("ESP32BLE", cg.Component)

GAPEventHandler = esp32_ble_ns.class_("GAPEventHandler")
GATTcEventHandler = esp32_ble_ns.class_("GATTcEventHandler")
GATTsEventHandler = esp32_ble_ns.class_("GATTsEventHandler")

BLEEnabledCondition = esp32_ble_ns.class_("BLEEnabledCondition", automation.Condition)
BLEEnableAction = esp32_ble_ns.class_("BLEEnableAction", automation.Action)
BLEDisableAction = esp32_ble_ns.class_("BLEDisableAction", automation.Action)

IoCapability = esp32_ble_ns.enum("IoCapability")
IO_CAPABILITY = {
    "none": IoCapability.IO_CAP_NONE,
    "keyboard_only": IoCapability.IO_CAP_IN,
    "keyboard_display": IoCapability.IO_CAP_KBDISP,
    "display_only": IoCapability.IO_CAP_OUT,
    "display_yes_no": IoCapability.IO_CAP_IO,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32BLE),
        cv.Optional(CONF_IO_CAPABILITY, default="none"): cv.enum(
            IO_CAPABILITY, lower=True
        ),
        cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


def validate_variant(_):
    variant = get_esp32_variant()
    if variant in NO_BLUETOOTH_VARIANTS:
        raise cv.Invalid(f"{variant} does not support Bluetooth")


FINAL_VALIDATE_SCHEMA = validate_variant


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_enable_on_boot(config[CONF_ENABLE_ON_BOOT]))
    cg.add(var.set_io_capability(config[CONF_IO_CAPABILITY]))
    await cg.register_component(var, config)

    if CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
        add_idf_sdkconfig_option("CONFIG_BT_BLE_42_FEATURES_SUPPORTED", True)


@automation.register_condition("ble.enabled", BLEEnabledCondition, cv.Schema({}))
async def ble_enabled_to_code(config, condition_id, template_arg, args):
    return cg.new_Pvariable(condition_id, template_arg)


@automation.register_action("ble.enable", BLEEnableAction, cv.Schema({}))
async def ble_enable_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)


@automation.register_action("ble.disable", BLEDisableAction, cv.Schema({}))
async def ble_disable_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)
