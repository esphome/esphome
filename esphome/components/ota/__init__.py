from esphome.cpp_generator import RawExpression
from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_NUM_ATTEMPTS,
    CONF_PASSWORD,
    CONF_PORT,
    CONF_REBOOT_TIMEOUT,
    CONF_SAFE_MODE,
)
from esphome.core import CORE, coroutine_with_priority

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["network"]

CONF_ON_BEGIN = "on_begin"
CONF_ON_PROGRESS = "on_progress"
CONF_ON_END = "on_end"
CONF_ON_ERROR = "on_error"
CONF_ON_BEGIN_TRIGGER_ID = "on_begin_trigger_id"
CONF_ON_PROGRESS_TRIGGER_ID = "on_progress_trigger_id"
CONF_ON_END_TRIGGER_ID = "on_end_trigger_id"
CONF_ON_ERROR_TRIGGER_ID = "on_error_trigger_id"

ota_ns = cg.esphome_ns.namespace("ota")
OTAComponent = ota_ns.class_("OTAComponent", cg.Component)
OTAStartTrigger = ota_ns.class_("OTAStartTrigger", automation.Trigger.template())
OTAProgressTrigger = ota_ns.class_("OTAProgressTrigger", automation.Trigger.template())
OTAEndTrigger = ota_ns.class_("OTAEndTrigger", automation.Trigger.template())
OTAErrorTrigger = ota_ns.class_("OTAErrorTrigger", automation.Trigger.template())

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OTAComponent),
        cv.Optional(CONF_SAFE_MODE, default=True): cv.boolean,
        cv.SplitDefault(CONF_PORT, esp8266=8266, esp32=3232): cv.port,
        cv.Optional(CONF_PASSWORD, default=""): cv.string,
        cv.Optional(
            CONF_REBOOT_TIMEOUT, default="5min"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_NUM_ATTEMPTS, default="10"): cv.positive_not_null_int,
        cv.Optional(CONF_ON_BEGIN): automation.validate_automation(
            {
                cv.GenerateID(CONF_ON_BEGIN_TRIGGER_ID): cv.declare_id(OTAStartTrigger),
            }
        ),
        cv.Optional(CONF_ON_ERROR): automation.validate_automation(
            {
                cv.GenerateID(CONF_ON_ERROR_TRIGGER_ID): cv.declare_id(OTAErrorTrigger),
            }
        ),
        cv.Optional(CONF_ON_PROGRESS): automation.validate_automation(
            {
                cv.GenerateID(CONF_ON_PROGRESS_TRIGGER_ID): cv.declare_id(
                    OTAProgressTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_END): automation.validate_automation(
            {
                cv.GenerateID(CONF_ON_END_TRIGGER_ID): cv.declare_id(OTAEndTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(50.0)
def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_auth_password(config[CONF_PASSWORD]))

    yield cg.register_component(var, config)

    if config[CONF_SAFE_MODE]:
        condition = var.should_enter_safe_mode(
            config[CONF_NUM_ATTEMPTS], config[CONF_REBOOT_TIMEOUT]
        )
        cg.add(RawExpression(f"if ({condition}) return"))

    for conf in config.get(CONF_ON_BEGIN, []):
        trigger = cg.new_Pvariable(conf[CONF_ON_BEGIN_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_PROGRESS, []):
        trigger = cg.new_Pvariable(conf[CONF_ON_PROGRESS_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [(float, "x")], conf)
    for conf in config.get(CONF_ON_END, []):
        trigger = cg.new_Pvariable(conf[CONF_ON_END_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_ERROR, []):
        trigger = cg.new_Pvariable(conf[CONF_ON_ERROR_TRIGGER_ID], var)
        yield automation.build_automation(trigger, [(int, "x")], conf)

    if CORE.is_esp8266:
        cg.add_library("Update", None)
    elif CORE.is_esp32:
        cg.add_library("Hash", None)
