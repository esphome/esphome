import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import switch
from esphome.const import (
    CONF_ID,
    CONF_MULTIPLIER,
    CONF_NAME,
    CONF_RUN_DURATION,
)

CODEOWNERS = ["@kbx81"]

CONF_AUTO_ADVANCE_SWITCH_NAME = "auto_advance_switch_name"
CONF_ENABLE_SWITCH_NAME = "enable_switch_name"
CONF_PUMP_SWITCH = "pump_switch"
CONF_REVERSE_SWITCH_NAME = "reverse_switch_name"
CONF_VALVE_OPEN_DELAY = "valve_open_delay"
CONF_VALVE_OVERLAP = "valve_overlap"
CONF_VALVE_NUMBER = "valve_number"
CONF_VALVE_SWITCH_NAME = "valve_switch_name"
CONF_VALVE_SWITCH = "valve_switch"
CONF_VALVES = "valves"

sprinkler_ns = cg.esphome_ns.namespace("sprinkler")
Sprinkler = sprinkler_ns.class_("Sprinkler")

SetMultiplierAction = sprinkler_ns.class_("SetMultiplierAction", automation.Action)
StartFullCycleAction = sprinkler_ns.class_("StartFullCycleAction", automation.Action)
StartSingleValveAction = sprinkler_ns.class_(
    "StartSingleValveAction", automation.Action
)
ShutdownAction = sprinkler_ns.class_("ShutdownAction", automation.Action)
NextValveAction = sprinkler_ns.class_("NextValveAction", automation.Action)
PreviousValveAction = sprinkler_ns.class_("PreviousValveAction", automation.Action)
PauseAction = sprinkler_ns.class_("PauseAction", automation.Action)
ResumeAction = sprinkler_ns.class_("ResumeAction", automation.Action)
ResumeOrStartAction = sprinkler_ns.class_("ResumeOrStartAction", automation.Action)


def validate_sprinkler(config):
    for valve_group in config:
        for valve in valve_group:
            if (
                CONF_VALVE_OVERLAP in config
                and valve[CONF_RUN_DURATION] <= config[CONF_VALVE_OVERLAP]
            ):
                raise cv.Invalid(
                    f"{CONF_RUN_DURATION} must be greater than {CONF_VALVE_OVERLAP}"
                )
            if (
                CONF_VALVE_OPEN_DELAY in config
                and valve[CONF_RUN_DURATION] <= config[CONF_VALVE_OPEN_DELAY]
            ):
                raise cv.Invalid(
                    f"{CONF_RUN_DURATION} must be greater than {CONF_VALVE_OPEN_DELAY}"
                )
    return config


SPRINKLER_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Sprinkler),
    }
)

SPRINKLER_ACTION_SINGLE_VALVE_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Sprinkler),
        cv.Required(CONF_VALVE_NUMBER): cv.templatable(cv.positive_int),
    }
)

SPRINKLER_ACTION_SET_MULTIPLIER_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Sprinkler),
        cv.Required(CONF_MULTIPLIER): cv.templatable(cv.positive_float),
    }
)

SPRINKLER_VALVE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ENABLE_SWITCH_NAME): cv.string,
        cv.Optional(CONF_PUMP_SWITCH): cv.use_id(switch.Switch),
        cv.Required(CONF_RUN_DURATION): cv.positive_time_period_seconds,
        cv.Required(CONF_VALVE_SWITCH_NAME): cv.string,
        cv.Required(CONF_VALVE_SWITCH): cv.use_id(switch.Switch),
    }
)

SPRINKLER_VALVE_GROUP_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Sprinkler),
        cv.Required(CONF_AUTO_ADVANCE_SWITCH_NAME): cv.string,
        cv.Required(CONF_NAME): cv.string,
        cv.Required(CONF_REVERSE_SWITCH_NAME): cv.string,
        cv.Exclusive(
            CONF_VALVE_OVERLAP, "open_delay/overlap"
        ): cv.positive_time_period_seconds,
        cv.Exclusive(
            CONF_VALVE_OPEN_DELAY, "open_delay/overlap"
        ): cv.positive_time_period_seconds,
        cv.Required(CONF_VALVES): cv.ensure_list(SPRINKLER_VALVE_SCHEMA),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.ensure_list(SPRINKLER_VALVE_GROUP_SCHEMA),
    validate_sprinkler,
)


@automation.register_action(
    "sprinkler.set_multiplier",
    SetMultiplierAction,
    SPRINKLER_ACTION_SET_MULTIPLIER_SCHEMA,
)
async def sprinkler_set_multiplier_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_MULTIPLIER], args, cg.float_)
    cg.add(var.set_multiplier(template_))
    return var


@automation.register_action(
    "sprinkler.start_full_cycle", StartFullCycleAction, SPRINKLER_ACTION_SCHEMA
)
async def sprinkler_start_full_cycle_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "sprinkler.start_single_valve",
    StartSingleValveAction,
    SPRINKLER_ACTION_SINGLE_VALVE_SCHEMA,
)
async def sprinkler_start_single_valve_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_VALVE_NUMBER], args, cg.uint8)
    cg.add(var.set_valve_to_start(template_))
    return var


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
    for valve_group in config:
        var = cg.new_Pvariable(valve_group[CONF_ID])
        cg.add(
            var.pre_setup(
                valve_group[CONF_NAME],
                valve_group[CONF_AUTO_ADVANCE_SWITCH_NAME],
                valve_group[CONF_REVERSE_SWITCH_NAME],
            )
        )
        for valve in valve_group[CONF_VALVES]:
            cg.add(
                var.add_valve(
                    valve[CONF_VALVE_SWITCH_NAME], valve[CONF_ENABLE_SWITCH_NAME]
                )
            )

        if CONF_VALVE_OVERLAP in valve_group:
            cg.add(var.set_valve_overlap(valve_group[CONF_VALVE_OVERLAP]))

        if CONF_VALVE_OPEN_DELAY in valve_group:
            cg.add(var.set_valve_open_delay(valve_group[CONF_VALVE_OPEN_DELAY]))

    for valve_group in config:
        var = await cg.get_variable(valve_group[CONF_ID])
        for valve_index, valve in enumerate(valve_group[CONF_VALVES]):
            valve_switch = await cg.get_variable(valve[CONF_VALVE_SWITCH])
            if CONF_PUMP_SWITCH in valve:
                pump = await cg.get_variable(valve[CONF_PUMP_SWITCH])
                cg.add(
                    var.configure_valve_switch(
                        valve_index, valve_switch, valve[CONF_RUN_DURATION], pump
                    )
                )
            else:
                cg.add(
                    var.configure_valve_switch(
                        valve_index, valve_switch, valve[CONF_RUN_DURATION]
                    )
                )

    for valve_group in config:
        var = await cg.get_variable(valve_group[CONF_ID])
        for controller_to_add in config:
            controller = await cg.get_variable(controller_to_add[CONF_ID])
            if var != controller:
                cg.add(var.add_controller(controller))
