from esphome import automation
import esphome.codegen as cg
from esphome.components.esp32 import add_idf_sdkconfig_option, const, get_esp32_variant
import esphome.config_validation as cv
from esphome.const import CONF_ENABLE_ON_BOOT, CONF_ESPHOME, CONF_ID, CONF_NAME
from esphome.core import CORE
from esphome.core.config import CONF_NAME_ADD_MAC_SUFFIX
import esphome.final_validate as fv

DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@jesserockz", "@Rapsssito"]

CONF_BLE_ID = "ble_id"
CONF_IO_CAPABILITY = "io_capability"
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
        cv.Optional(CONF_NAME): cv.All(cv.string, cv.Length(max=20)),
        cv.Optional(CONF_IO_CAPABILITY, default="none"): cv.enum(
            IO_CAPABILITY, lower=True
        ),
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


def final_validation(config):
    validate_variant(config)
    if (name := config.get(CONF_NAME)) is not None:
        full_config = fv.full_config.get()
        max_length = 20
        if full_config[CONF_ESPHOME][CONF_NAME_ADD_MAC_SUFFIX]:
            max_length -= 7  # "-AABBCC" is appended when add mac suffix option is used
        if len(name) > max_length:
            raise cv.Invalid(
                f"Name '{name}' is too long, maximum length is {max_length} characters"
            )

    return config


FINAL_VALIDATE_SCHEMA = final_validation


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_enable_on_boot(config[CONF_ENABLE_ON_BOOT]))
    cg.add(var.set_io_capability(config[CONF_IO_CAPABILITY]))
    cg.add(var.set_advertising_cycle_time(config[CONF_ADVERTISING_CYCLE_TIME]))
    if (name := config.get(CONF_NAME)) is not None:
        cg.add(var.set_name(name))
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
