from dataclasses import dataclass

from esphome import automation
from esphome.automation import Trigger
import esphome.codegen as cg
from esphome.components import key_provider
import esphome.config_validation as cv
from esphome.const import (
    CONF_ENABLE_ON_BOOT,
    CONF_ID,
    CONF_MAX_LENGTH,
    CONF_MIN_LENGTH,
    CONF_ON_TIMEOUT,
    CONF_SOURCE_ID,
    CONF_TIMEOUT,
    CONF_TRIGGER_ID,
)
from esphome.cpp_generator import MockObj, literal
from esphome.types import TemplateArgsType

CODEOWNERS = ["@ssieb"]

MULTI_CONF = True

AUTO_LOAD = ["key_provider"]

CONF_START_KEYS = "start_keys"
CONF_END_KEYS = "end_keys"
CONF_END_KEY_REQUIRED = "end_key_required"
CONF_BACK_KEYS = "back_keys"
CONF_CLEAR_KEYS = "clear_keys"
CONF_ALLOWED_KEYS = "allowed_keys"
CONF_ON_PROGRESS = "on_progress"
CONF_ON_RESULT = "on_result"

key_collector_ns = cg.esphome_ns.namespace("key_collector")
KeyCollector = key_collector_ns.class_("KeyCollector", cg.Component)
EnableAction = key_collector_ns.class_("EnableAction", automation.Action)
DisableAction = key_collector_ns.class_("DisableAction", automation.Action)

X_TYPE = cg.std_string_ref.operator("const")


@dataclass
class Argument:
    type: MockObj
    name: str


TRIGGER_TYPES = {
    CONF_ON_PROGRESS: [Argument(X_TYPE, "x"), Argument(cg.uint8, "start")],
    CONF_ON_RESULT: [
        Argument(X_TYPE, "x"),
        Argument(cg.uint8, "start"),
        Argument(cg.uint8, "end"),
    ],
    CONF_ON_TIMEOUT: [Argument(X_TYPE, "x"), Argument(cg.uint8, "start")],
}

CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(KeyCollector),
            cv.Optional(CONF_SOURCE_ID): cv.ensure_list(
                cv.use_id(key_provider.KeyProvider)
            ),
            cv.Optional(CONF_MIN_LENGTH): cv.uint16_t,
            cv.Optional(CONF_MAX_LENGTH): cv.uint16_t,
            cv.Optional(CONF_START_KEYS): cv.string,
            cv.Optional(CONF_END_KEYS): cv.string,
            cv.Optional(CONF_END_KEY_REQUIRED): cv.boolean,
            cv.Optional(CONF_BACK_KEYS): cv.string,
            cv.Optional(CONF_CLEAR_KEYS): cv.string,
            cv.Optional(CONF_ALLOWED_KEYS): cv.string,
            **{
                cv.Optional(trigger_type): automation.validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                            Trigger.template(*[arg.type for arg in args])
                        ),
                    }
                )
                for trigger_type, args in TRIGGER_TYPES.items()
            },
            cv.Optional(CONF_TIMEOUT): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
        }
    ),
    cv.has_at_least_one_key(CONF_END_KEYS, CONF_MAX_LENGTH),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    for source_conf in config.get(CONF_SOURCE_ID, ()):
        source = await cg.get_variable(source_conf)
        cg.add(var.add_provider(source))
    if CONF_MIN_LENGTH in config:
        cg.add(var.set_min_length(config[CONF_MIN_LENGTH]))
    if CONF_MAX_LENGTH in config:
        cg.add(var.set_max_length(config[CONF_MAX_LENGTH]))
    if CONF_START_KEYS in config:
        cg.add(var.set_start_keys(config[CONF_START_KEYS]))
    if CONF_END_KEYS in config:
        cg.add(var.set_end_keys(config[CONF_END_KEYS]))
    if CONF_END_KEY_REQUIRED in config:
        cg.add(var.set_end_key_required(config[CONF_END_KEY_REQUIRED]))
    if CONF_BACK_KEYS in config:
        cg.add(var.set_back_keys(config[CONF_BACK_KEYS]))
    if CONF_CLEAR_KEYS in config:
        cg.add(var.set_clear_keys(config[CONF_CLEAR_KEYS]))
    if CONF_ALLOWED_KEYS in config:
        cg.add(var.set_allowed_keys(config[CONF_ALLOWED_KEYS]))

    for trigger_name, args in TRIGGER_TYPES.items():
        arglist: TemplateArgsType = [(arg.type, arg.name) for arg in args]
        for conf in config.get(trigger_name, ()):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
            add_trig = getattr(
                var,
                f"add_on_{trigger_name.rsplit('_', maxsplit=1)[-1].lower()}_callback",
            )
            await automation.build_automation(
                trigger,
                arglist,
                conf,
            )
            lamb = trigger.trigger(*[literal(arg.name) for arg in args])
            cg.add(add_trig(await cg.process_lambda(lamb, arglist)))

    if timeout := config.get(CONF_TIMEOUT):
        cg.add(var.set_timeout(timeout))
    cg.add(var.set_enabled(config[CONF_ENABLE_ON_BOOT]))


@automation.register_action(
    "key_collector.enable",
    EnableAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(KeyCollector),
        }
    ),
)
async def enable_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "key_collector.disable",
    DisableAction,
    automation.maybe_simple_id(
        {
            cv.GenerateID(): cv.use_id(KeyCollector),
        }
    ),
)
async def disable_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
