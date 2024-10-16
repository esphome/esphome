from esphome import automation
import esphome.codegen as cg
from esphome.components.esp32 import add_idf_sdkconfig_option, const, get_esp32_variant
import esphome.config_validation as cv
from esphome.const import CONF_ENABLE_ON_BOOT, CONF_ID
from esphome.core import CORE

DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@jesserockz", "@Rapsssito"]

CONF_BLE_ID = "ble_id"
CONF_IO_CAPABILITY = "io_capability"
CONF_AUTH_REQ_MODE = "auth_req_mode"
CONF_MAX_KEY_SIZE = "max_key_size"
CONF_MIN_KEY_SIZE = "min_key_size"
CONF_ADVERTISING_CYCLE_TIME = "advertising_cycle_time"

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

AuthReqMode = esp32_ble_ns.enum("AuthReqMode")
AUTH_REQ_MODE = {
    "no_bond": AuthReqMode.AUTH_REQ_NO_BOND,
    "bond": AuthReqMode.AUTH_REQ_BOND,
    "mitm": AuthReqMode.AUTH_REQ_MITM,
    "bond_mitm": AuthReqMode.AUTH_REQ_BOND_MITM,
    "sc_only": AuthReqMode.AUTH_REQ_SC_ONLY,
    "sc_bond": AuthReqMode.AUTH_REQ_SC_BOND,
    "sc_mitm": AuthReqMode.AUTH_REQ_SC_MITM,
    "sc_mitm_bond": AuthReqMode.AUTH_REQ_SC_MITM_BOND,
}

esp_power_level_t = cg.global_ns.enum("esp_power_level_t")

TX_POWER_LEVELS = {
    -12: esp_power_level_t.ESP_PWR_LVL_N12,
    -9: esp_power_level_t.ESP_PWR_LVL_N9,
    -6: esp_power_level_t.ESP_PWR_LVL_N6,
    -3: esp_power_level_t.ESP_PWR_LVL_N3,
    0: esp_power_level_t.ESP_PWR_LVL_N0,
    3: esp_power_level_t.ESP_PWR_LVL_P3,
    6: esp_power_level_t.ESP_PWR_LVL_P6,
    9: esp_power_level_t.ESP_PWR_LVL_P9,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32BLE),
        cv.Optional(CONF_IO_CAPABILITY, default="none"): cv.enum(
            IO_CAPABILITY, lower=True
        ),
        cv.Optional(CONF_AUTH_REQ_MODE, default="no_bond"): cv.enum(
            AUTH_REQ_MODE, lower=True
        ),
        cv.Optional(CONF_MAX_KEY_SIZE, default="16"): cv.int_range(min=7, max=16),
        cv.Optional(CONF_MIN_KEY_SIZE, default="7"): cv.int_range(min=7, max=16),
        cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
        cv.Optional(
            CONF_ADVERTISING_CYCLE_TIME, default="10s"
        ): cv.positive_time_period_milliseconds,
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
    if CONF_AUTH_REQ_MODE in config:
        cg.add(var.set_auth_req(config[CONF_AUTH_REQ_MODE]))
    if CONF_MAX_KEY_SIZE in config:
        cg.add(var.set_max_key_size(config[CONF_MAX_KEY_SIZE]))
    if CONF_MIN_KEY_SIZE in config:
        cg.add(var.set_min_key_size(config[CONF_MIN_KEY_SIZE]))

    cg.add(var.set_advertising_cycle_time(config[CONF_ADVERTISING_CYCLE_TIME]))
    await cg.register_component(var, config)

    if CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
        add_idf_sdkconfig_option("CONFIG_BT_BLE_42_FEATURES_SUPPORTED", True)

    cg.add_define("USE_ESP32_BLE")


@automation.register_condition("ble.enabled", BLEEnabledCondition, cv.Schema({}))
async def ble_enabled_to_code(config, condition_id, template_arg, args):
    return cg.new_Pvariable(condition_id, template_arg)


@automation.register_action("ble.enable", BLEEnableAction, cv.Schema({}))
async def ble_enable_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)


@automation.register_action("ble.disable", BLEDisableAction, cv.Schema({}))
async def ble_disable_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)
