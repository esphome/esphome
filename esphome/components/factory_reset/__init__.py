from esphome.automation import Trigger, build_automation, validate_automation
import esphome.codegen as cg
from esphome.components.esp8266 import CONF_RESTORE_FROM_FLASH, KEY_ESP8266
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
    PLATFORM_LN882X,
    PLATFORM_RTL87XX,
)
from esphome.core import CORE
from esphome.final_validate import full_config

CODEOWNERS = ["@anatoly-savchenkov"]

factory_reset_ns = cg.esphome_ns.namespace("factory_reset")
FactoryResetComponent = factory_reset_ns.class_("FactoryResetComponent", cg.Component)
FastBootTrigger = factory_reset_ns.class_("FastBootTrigger", Trigger, cg.Component)

CONF_MAX_DELAY = "max_delay"
CONF_RESETS_REQUIRED = "resets_required"
CONF_ON_INCREMENT = "on_increment"


def _validate(config):
    if CONF_RESETS_REQUIRED in config:
        return cv.only_on(
            [
                PLATFORM_BK72XX,
                PLATFORM_ESP32,
                PLATFORM_ESP8266,
                PLATFORM_LN882X,
                PLATFORM_RTL87XX,
            ]
        )(config)

    if CONF_ON_INCREMENT in config:
        raise cv.Invalid(
            f"'{CONF_ON_INCREMENT}' requires a value for '{CONF_RESETS_REQUIRED}'"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FactoryResetComponent),
            cv.Optional(CONF_MAX_DELAY, default="10s"): cv.All(
                cv.positive_time_period_seconds,
                cv.Range(min=cv.TimePeriod(milliseconds=1000)),
            ),
            cv.Optional(CONF_RESETS_REQUIRED): cv.positive_not_null_int,
            cv.Optional(CONF_ON_INCREMENT): validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(FastBootTrigger),
                }
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate,
)


def _final_validate(config):
    if CORE.is_esp8266 and CONF_RESETS_REQUIRED in config:
        fconfig = full_config.get()
        if not fconfig.get_config_for_path([KEY_ESP8266, CONF_RESTORE_FROM_FLASH]):
            raise cv.Invalid(
                "'resets_required' needs 'restore_from_flash' to be enabled in the  'esp8266' configuration"
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    if reset_count := config.get(CONF_RESETS_REQUIRED):
        var = cg.new_Pvariable(
            config[CONF_ID],
            reset_count,
            config[CONF_MAX_DELAY].total_milliseconds,
        )
        await cg.register_component(var, config)
        for conf in config.get(CONF_ON_INCREMENT, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await build_automation(
                trigger,
                [
                    (cg.uint8, "x"),
                    (cg.uint8, "target"),
                ],
                conf,
            )
