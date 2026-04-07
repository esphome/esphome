import logging

from esphome import automation
import esphome.codegen as cg
from esphome.components import button, text_sensor
from esphome.components.esp32 import (
    add_idf_sdkconfig_option,
    const as esp32_const,
    get_esp32_variant,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_DELAY,
    CONF_ENABLE_ON_BOOT,
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_NUM_SCANS,
    CONF_TRIGGER_ID,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_RESTART,
)
from esphome.core import CORE

from .const import (
    CONF_LAST_ERROR,
    CONF_ON_SCAN_RESULT,
    CONF_ON_SCAN_START,
    CONF_RESET_BT_STACK,
)

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["text_sensor"]
DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@RoboMagus"]

NO_BLUETOOTH_VARIANTS = [esp32_const.VARIANT_ESP32S2]

esp32_bt_classic_ns = cg.esphome_ns.namespace("esp32_bt_classic")
ESP32BtClassic = esp32_bt_classic_ns.class_("ESP32BtClassic", cg.Component)

BTClassicEnableAction = esp32_bt_classic_ns.class_(
    "BTClassicEnableAction", automation.Action
)
BTClassicDisableAction = esp32_bt_classic_ns.class_(
    "BTClassicDisableAction", automation.Action
)

BtAddress = esp32_bt_classic_ns.class_("BtAddress")
BtAddressConstRef = BtAddress.operator("ref").operator("const")

BtStatus = esp32_bt_classic_ns.class_("BtStatus")
BtStatusConstRef = BtStatus.operator("ref").operator("const")

ScanStatus = esp32_bt_classic_ns.class_("ScanStatus")
ScanStatusConstRef = ScanStatus.operator("ref").operator("const")

GAPEventHandler = esp32_bt_classic_ns.class_("GAPEventHandler")

# Actions
BtClassicScanAction = esp32_bt_classic_ns.class_(
    "BtClassicScanAction", automation.Action
)
# Triggers
BtClassicScanResultTrigger = esp32_bt_classic_ns.class_(
    "BtClassicScanResultTrigger",
    automation.Trigger.template(
        BtAddress, BtStatusConstRef, cg.const_char_ptr, ScanStatusConstRef
    ),
)
BtClassicScanStartTrigger = esp32_bt_classic_ns.class_(
    "BtClassicScanStartTrigger", automation.Trigger.template()
)

ResetBtStackButton = esp32_bt_classic_ns.class_("ResetBtStackButton", button.Button)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESP32BtClassic),
            cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
            cv.Optional(CONF_ON_SCAN_START): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        BtClassicScanStartTrigger
                    )
                }
            ),
            cv.Optional(CONF_ON_SCAN_RESULT): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        BtClassicScanResultTrigger
                    ),
                    cv.Optional(CONF_MAC_ADDRESS): cv.ensure_list(cv.mac_address),
                }
            ),
            cv.Optional(CONF_LAST_ERROR): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_RESET_BT_STACK): button.button_schema(
                ResetBtStackButton,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon=ICON_RESTART,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
)


def validate_variant(_):
    variant = get_esp32_variant()
    if variant in NO_BLUETOOTH_VARIANTS:
        raise cv.Invalid(f"{variant} does not support Bluetooth")


FINAL_VALIDATE_SCHEMA = validate_variant


BT_CLASSIC_SCAN_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(ESP32BtClassic),
        cv.Required(CONF_MAC_ADDRESS): cv.templatable(cv.ensure_list(cv.mac_address)),
        cv.Optional(CONF_NUM_SCANS, default=1): cv.templatable(cv.uint8_t),
        cv.Optional(CONF_DELAY, default="0s"): cv.positive_time_period_milliseconds,
    }
)


@automation.register_action(
    "bt_classic.bt_classic_scan", BtClassicScanAction, BT_CLASSIC_SCAN_ACTION_SCHEMA
)
async def bt_classic_scan_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    addr = config[CONF_MAC_ADDRESS]
    if cg.is_template(addr):
        templ = await cg.templatable(addr, args, cg.std_vector.template(cg.std_string))
        cg.add(var.set_addr_template(templ))
    else:
        addr_list = []
        for it in addr:
            addr_list.append(it.as_hex)
        cg.add(var.set_addr_simple(addr_list))

    if CONF_NUM_SCANS in config:
        if cg.is_template(config[CONF_NUM_SCANS]):
            templ = await cg.templatable(config[CONF_NUM_SCANS], args, cg.uint8)
            cg.add(var.set_num_scans_template(templ))
        else:
            cg.add(var.set_num_scans_simple(config[CONF_NUM_SCANS]))

    if CONF_DELAY in config:
        cg.add(var.set_scan_delay(config[CONF_DELAY]))

    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_enable_on_boot(config[CONF_ENABLE_ON_BOOT]))

    for conf in config.get(CONF_ON_SCAN_START, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_SCAN_RESULT, []):
        mac_addr = []
        for it in conf.get(CONF_MAC_ADDRESS, []):
            mac_addr.append(it.as_hex)

        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var, mac_addr)
        await automation.build_automation(
            trigger,
            [
                (BtAddressConstRef, "address"),
                (BtStatusConstRef, "scan_result"),
                (cg.const_char_ptr, "name"),
                (ScanStatusConstRef, "status"),
            ],
            conf,
        )

    if CONF_LAST_ERROR in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LAST_ERROR])
        cg.add(var.set_last_error_sensor(sens))

    if revert_config := config.get(CONF_RESET_BT_STACK):
        b = await button.new_button(revert_config)
        await cg.register_parented(b, var)
        cg.add(var.set_reset_bt_stack_button(b))

    add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BTDM_CTRL_MODE_BTDM", True)
    add_idf_sdkconfig_option("CONFIG_BT_CLASSIC_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BTU_TASK_STACK_SIZE", 8192)

    add_idf_sdkconfig_option("CONFIG_BT_LOG_GAP_TRACE_LEVEL_DEBUG", True)
    add_idf_sdkconfig_option("CONFIG_BT_LOG_GAP_TRACE_LEVEL", 5)

    add_idf_sdkconfig_option("CONFIG_BT_LOG_BTC_TRACE_LEVEL_DEBUG", True)
    add_idf_sdkconfig_option("CONFIG_BT_LOG_BTC_TRACE_LEVEL", 5)

    cg.add_define("DISABLE_BT_CLASSIC_MEM_RELEASE")
    cg.add_build_flag("-DBT_CONTROLLER_MODE=ESP_BT_MODE_BTDM")


@automation.register_action(
    "bt_classic.enable", BTClassicEnableAction, cv.Schema({}), synchronous=True
)
async def bt_classic_enable_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)


@automation.register_action(
    "bt_classic.disable", BTClassicDisableAction, cv.Schema({}), synchronous=True
)
async def bt_classic_disable_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)
