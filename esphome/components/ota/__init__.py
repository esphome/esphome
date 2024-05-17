import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.core import CORE, coroutine_with_priority

from esphome.const import CONF_ESPHOME, CONF_OTA, CONF_PLATFORM, CONF_TRIGGER_ID

CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["md5"]

IS_PLATFORM_COMPONENT = True

CONF_ON_ABORT = "on_abort"
CONF_ON_BEGIN = "on_begin"
CONF_ON_END = "on_end"
CONF_ON_ERROR = "on_error"
CONF_ON_PROGRESS = "on_progress"
CONF_ON_STATE_CHANGE = "on_state_change"


ota_ns = cg.esphome_ns.namespace("ota")
OTAComponent = ota_ns.class_("OTAComponent", cg.Component)
OTAState = ota_ns.enum("OTAState")
OTAAbortTrigger = ota_ns.class_("OTAAbortTrigger", automation.Trigger.template())
OTAEndTrigger = ota_ns.class_("OTAEndTrigger", automation.Trigger.template())
OTAErrorTrigger = ota_ns.class_("OTAErrorTrigger", automation.Trigger.template())
OTAProgressTrigger = ota_ns.class_("OTAProgressTrigger", automation.Trigger.template())
OTAStartTrigger = ota_ns.class_("OTAStartTrigger", automation.Trigger.template())
OTAStateChangeTrigger = ota_ns.class_(
    "OTAStateChangeTrigger", automation.Trigger.template()
)


def _ota_final_validate(config):
    if len(config) < 1:
        raise cv.Invalid(
            f"At least one platform must be specified for '{CONF_OTA}'; add '{CONF_PLATFORM}: {CONF_ESPHOME}' for original OTA functionality"
        )


FINAL_VALIDATE_SCHEMA = _ota_final_validate

BASE_OTA_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ON_STATE_CHANGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAStateChangeTrigger),
            }
        ),
        cv.Optional(CONF_ON_ABORT): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAAbortTrigger),
            }
        ),
        cv.Optional(CONF_ON_BEGIN): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAStartTrigger),
            }
        ),
        cv.Optional(CONF_ON_END): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OTAEndTrigger),
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
    }
)


@coroutine_with_priority(51.0)
async def to_code(config):
    cg.add_define("USE_OTA")

    if CORE.is_esp32 and CORE.using_arduino:
        cg.add_library("Update", None)

    if CORE.is_rp2040 and CORE.using_arduino:
        cg.add_library("Updater", None)


async def ota_to_code(var, config):
    use_state_callback = False
    for conf in config.get(CONF_ON_STATE_CHANGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(OTAState, "state")], conf)
        use_state_callback = True
    for conf in config.get(CONF_ON_ABORT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
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
