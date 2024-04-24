from esphome.cpp_generator import RawExpression
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_NUM_ATTEMPTS,
    CONF_PASSWORD,
    CONF_PORT,
    CONF_REBOOT_TIMEOUT,
    CONF_SAFE_MODE,
    CONF_TRIGGER_ID,
    CONF_OTA,
    KEY_PAST_SAFE_MODE,
    CONF_VERSION,
)
from esphome.core import CORE, coroutine_with_priority
import esphome.final_validate as fv
from esphome.components.zephyr.const import BOOTLOADER_MCUBOOT

CODEOWNERS = ["@esphome/core"]


def AUTO_LOAD():
    if CORE.using_zephyr:
        return ["zephyr_ota_mcumgr"]
    return ["ota_network"]


CONF_ON_STATE_CHANGE = "on_state_change"
CONF_ON_BEGIN = "on_begin"
CONF_ON_PROGRESS = "on_progress"
CONF_ON_END = "on_end"
CONF_ON_ERROR = "on_error"

ota_ns = cg.esphome_ns.namespace("ota")
OTAState = ota_ns.enum("OTAState")
if CORE.using_zephyr:
    OTAComponent = cg.esphome_ns.namespace("zephyr_ota_mcumgr").class_(
        "OTAComponent", cg.Component
    )
else:
    OTAComponent = cg.esphome_ns.namespace("ota_network").class_(
        "OTAComponent", cg.Component
    )
OTAStateChangeTrigger = ota_ns.class_(
    "OTAStateChangeTrigger", automation.Trigger.template()
)
OTAStartTrigger = ota_ns.class_("OTAStartTrigger", automation.Trigger.template())
OTAProgressTrigger = ota_ns.class_("OTAProgressTrigger", automation.Trigger.template())
OTAEndTrigger = ota_ns.class_("OTAEndTrigger", automation.Trigger.template())
OTAErrorTrigger = ota_ns.class_("OTAErrorTrigger", automation.Trigger.template())


def _not_supported_by_zephyr(value):
    if CORE.using_zephyr:
        raise cv.Invalid(f"Not supported by zephyr framework({value})")
    return value


def _default_ota_version():
    if CORE.using_zephyr:
        return cv.UNDEFINED
    return 2


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OTAComponent),
        cv.Optional(CONF_SAFE_MODE, default=True): cv.boolean,
        cv.Optional(CONF_VERSION, default=_default_ota_version()): cv.All(
            cv.one_of(1, 2, int=True), _not_supported_by_zephyr
        ),
        cv.SplitDefault(
            CONF_PORT,
            esp8266=8266,
            esp32=3232,
            rp2040=2040,
            bk72xx=8892,
            rtl87xx=8892,
        ): cv.All(
            cv.port,
            _not_supported_by_zephyr,
        ),
        cv.Optional(CONF_PASSWORD): cv.All(cv.string, _not_supported_by_zephyr),
        cv.Optional(
            CONF_REBOOT_TIMEOUT, default="5min"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_NUM_ATTEMPTS, default="10"): cv.positive_not_null_int,
        cv.Optional(CONF_ON_STATE_CHANGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAStateChangeTrigger),
            }
        ),
        cv.Optional(CONF_ON_BEGIN): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAStartTrigger),
            }
        ),
        cv.Optional(CONF_ON_ERROR): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAErrorTrigger),
            }
        ),
        cv.Optional(CONF_ON_PROGRESS): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAProgressTrigger),
            }
        ),
        cv.Optional(CONF_ON_END): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAEndTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def _validate_mcumgr(config):
    if CORE.using_zephyr:
        fconf = fv.full_config.get()
        try:
            bootloader = fconf.get_config_for_path(["nrf52", "bootloader"])
            if bootloader != BOOTLOADER_MCUBOOT:
                raise cv.Invalid(f"'{bootloader}' bootloader does not support OTA")
        except KeyError:
            pass


FINAL_VALIDATE_SCHEMA = _validate_mcumgr


@coroutine_with_priority(50.0)
async def to_code(config):
    CORE.data[CONF_OTA] = {}

    var = cg.new_Pvariable(config[CONF_ID])
    cg.add_define("USE_OTA")
    if not CORE.using_zephyr:
        cg.add(var.set_port(config[CONF_PORT]))
        if CONF_PASSWORD in config:
            cg.add(var.set_auth_password(config[CONF_PASSWORD]))
            cg.add_define("USE_OTA_PASSWORD")
        cg.add_define("USE_OTA_VERSION", config[CONF_VERSION])

    await cg.register_component(var, config)

    if config[CONF_SAFE_MODE]:
        condition = var.should_enter_safe_mode(
            config[CONF_NUM_ATTEMPTS], config[CONF_REBOOT_TIMEOUT]
        )
        cg.add(RawExpression(f"if ({condition}) return"))
        CORE.data[CONF_OTA][KEY_PAST_SAFE_MODE] = True

    if CORE.is_esp32 and CORE.using_arduino:
        cg.add_library("Update", None)

    if CORE.is_rp2040 and CORE.using_arduino:
        cg.add_library("Updater", None)

    use_state_callback = False
    for conf in config.get(CONF_ON_STATE_CHANGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(OTAState, "state")], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_BEGIN, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_PROGRESS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(float, "x")], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_END, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_ERROR, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.uint8, "x")], conf)
        use_state_callback = True
    if use_state_callback:
        cg.add_define("USE_OTA_STATE_CALLBACK")
