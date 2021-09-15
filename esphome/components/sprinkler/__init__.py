import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import switch
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_RUN_DURATION,
)

CODEOWNERS = ["@kbx81"]

CONF_PUMP = "pump"
CONF_ENABLE_SWITCH = "enable_switch"
CONF_VALVE_SWITCH = "valve_switch"
CONF_VALVE_OPEN_DELAY = "valve_open_delay"
CONF_VALVES = "valves"

sprinkler_ns = cg.esphome_ns.namespace("sprinkler")
Sprinkler = sprinkler_ns.class_("Sprinkler")

StartFullCycleAction = sprinkler_ns.class_("StartFullCycleAction", automation.Action)
ShutdownAction = sprinkler_ns.class_("ShutdownAction", automation.Action)
NextValveAction = sprinkler_ns.class_("NextValveAction", automation.Action)
PreviousValveAction = sprinkler_ns.class_("PreviousValveAction", automation.Action)
PauseAction = sprinkler_ns.class_("PauseAction", automation.Action)
ResumeAction = sprinkler_ns.class_("ResumeAction", automation.Action)
ResumeOrStartAction = sprinkler_ns.class_("ResumeOrStartAction", automation.Action)


def validate_sprinkler(config):
    for valve in config[CONF_VALVES]:
        if valve[CONF_RUN_DURATION] <= config[CONF_VALVE_OPEN_DELAY]:
            raise cv.Invalid(
                f"{CONF_RUN_DURATION} must be greater than {CONF_VALVE_OPEN_DELAY}"
            )
    return config


SPRINKLER_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Sprinkler),
    }
)


SPRINKLER_VALVE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_RUN_DURATION): cv.positive_time_period_seconds,
        cv.Required(CONF_VALVE_SWITCH): cv.use_id(switch.Switch),
        cv.Optional(CONF_ENABLE_SWITCH): cv.use_id(switch.Switch),
        cv.Optional(CONF_NAME): cv.string,
    }
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Sprinkler),
            cv.Optional(CONF_PUMP): cv.use_id(switch.Switch),
            cv.Required(CONF_VALVE_OPEN_DELAY): cv.positive_time_period_seconds,
            cv.Required(CONF_VALVES): cv.ensure_list(SPRINKLER_VALVE_SCHEMA),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_sprinkler,
)


@automation.register_action(
    "sprinkler.start_full_cycle", StartFullCycleAction, SPRINKLER_ACTION_SCHEMA
)
async def sprinkler_start_full_cycle_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "sprinkler.shutdown", ShutdownAction, SPRINKLER_ACTION_SCHEMA
)
async def sprinkler_shutdown_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "sprinkler.next_valve", NextValveAction, SPRINKLER_ACTION_SCHEMA
)
async def sprinkler_next_valve_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "sprinkler.previous_valve", PreviousValveAction, SPRINKLER_ACTION_SCHEMA
)
async def sprinkler_previous_valve_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("sprinkler.pause", PauseAction, SPRINKLER_ACTION_SCHEMA)
async def sprinkler_pause_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("sprinkler.resume", ResumeAction, SPRINKLER_ACTION_SCHEMA)
async def sprinkler_resume_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "sprinkler.resume_or_start_full_cycle", ResumeOrStartAction, SPRINKLER_ACTION_SCHEMA
)
async def sprinkler_resume_or_start_full_cycle_to_code(
    config, action_id, template_arg, args
):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    cg.add(var.set_valve_open_delay(config[CONF_VALVE_OPEN_DELAY]))

    if CONF_PUMP in config:
        pump = await cg.get_variable(config[CONF_PUMP])
        cg.add(var.set_pump_switch(pump))

    for valve in config[CONF_VALVES]:
        valve_switch = await cg.get_variable(valve[CONF_VALVE_SWITCH])
        if CONF_ENABLE_SWITCH in valve:
            enable_switch = await cg.get_variable(valve[CONF_ENABLE_SWITCH])
            cg.add(
                var.add_valve(
                    valve_switch,
                    enable_switch,
                    valve[CONF_RUN_DURATION],
                    valve[CONF_NAME],
                )
            )
        else:
            cg.add(
                var.add_valve(valve_switch, valve[CONF_RUN_DURATION], valve[CONF_NAME])
            )
