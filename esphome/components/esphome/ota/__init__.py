from esphome.cpp_generator import RawExpression
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_NUM_ATTEMPTS,
    CONF_OTA,
    CONF_PASSWORD,
    CONF_PORT,
    CONF_REBOOT_TIMEOUT,
    CONF_SAFE_MODE,
    CONF_TRIGGER_ID,
    CONF_VERSION,
    KEY_PAST_SAFE_MODE,
)
from esphome.core import CORE, coroutine_with_priority


CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["md5", "socket"]
DEPENDENCIES = ["network", "ota"]

CONF_ON_BEGIN = "on_begin"
CONF_ON_END = "on_end"
CONF_ON_ERROR = "on_error"
CONF_ON_PROGRESS = "on_progress"
CONF_ON_STATE_CHANGE = "on_state_change"

ota_esphome = cg.esphome_ns.namespace("ota_esphome")

OTAESPHomeComponent = ota_esphome.class_("OTAESPHomeComponent", cg.Component)
OTAESPHomeEndTrigger = ota_esphome.class_(
    "OTAESPHomeEndTrigger", automation.Trigger.template()
)
OTAESPHomeErrorTrigger = ota_esphome.class_(
    "OTAESPHomeErrorTrigger", automation.Trigger.template()
)
OTAESPHomeProgressTrigger = ota_esphome.class_(
    "OTAESPHomeProgressTrigger", automation.Trigger.template()
)
OTAESPHomeStartTrigger = ota_esphome.class_(
    "OTAESPHomeStartTrigger", automation.Trigger.template()
)
OTAESPHomeStateChangeTrigger = ota_esphome.class_(
    "OTAESPHomeStateChangeTrigger", automation.Trigger.template()
)

OTAESPHomeState = ota_esphome.enum("OTAESPHomeState")


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OTAESPHomeComponent),
        cv.Optional(CONF_SAFE_MODE, default=True): cv.boolean,
        cv.Optional(CONF_VERSION, default=2): cv.one_of(1, 2, int=True),
        cv.SplitDefault(
            CONF_PORT,
            esp8266=8266,
            esp32=3232,
            rp2040=2040,
            bk72xx=8892,
            rtl87xx=8892,
        ): cv.port,
        cv.Optional(CONF_PASSWORD): cv.string,
        cv.Optional(
            CONF_REBOOT_TIMEOUT, default="5min"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_NUM_ATTEMPTS, default="10"): cv.positive_not_null_int,
        cv.Optional(CONF_ON_STATE_CHANGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    OTAESPHomeStateChangeTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_BEGIN): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAESPHomeStartTrigger),
            }
        ),
        cv.Optional(CONF_ON_END): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAESPHomeEndTrigger),
            }
        ),
        cv.Optional(CONF_ON_ERROR): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAESPHomeErrorTrigger),
            }
        ),
        cv.Optional(CONF_ON_PROGRESS): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    OTAESPHomeProgressTrigger
                ),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(50.0)
async def to_code(config):
    CORE.data[CONF_OTA] = {}

    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add_define("USE_OTA")
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
        await automation.build_automation(trigger, [(OTAESPHomeState, "state")], conf)
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
