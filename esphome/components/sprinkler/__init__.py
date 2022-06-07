import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import switch
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_REPEAT,
    CONF_RUN_DURATION,
)

CODEOWNERS = ["@kbx81"]

CONF_AUTO_ADVANCE_SWITCH_ID = "auto_advance_switch_id"
CONF_AUTO_ADVANCE_SWITCH_NAME = "auto_advance_switch_name"
CONF_ENABLE_SWITCH_NAME = "enable_switch_name"
CONF_MAIN_SWITCH_ID = "main_switch_id"
CONF_MANUAL_SELECTION_DELAY = "manual_selection_delay"
CONF_MULTIPLIER = "multiplier"
CONF_PUMP_SWITCH = "pump_switch"
CONF_PUMP_SWITCH_OFF_DURING_VALVE_OPEN_DELAY = "pump_switch_off_during_valve_open_delay"
CONF_REVERSE_SWITCH_ID = "reverse_switch_id"
CONF_REVERSE_SWITCH_NAME = "reverse_switch_name"
CONF_VALVE_OPEN_DELAY = "valve_open_delay"
CONF_VALVE_OVERLAP = "valve_overlap"
CONF_VALVE_NUMBER = "valve_number"
CONF_VALVE_SWITCH_NAME = "valve_switch_name"
CONF_VALVE_SWITCH = "valve_switch"
CONF_VALVES = "valves"

sprinkler_ns = cg.esphome_ns.namespace("sprinkler")
Sprinkler = sprinkler_ns.class_("Sprinkler", cg.Component)
SprinklerSwitch = sprinkler_ns.class_("SprinklerSwitch", switch.Switch, cg.Component)

SetMultiplierAction = sprinkler_ns.class_("SetMultiplierAction", automation.Action)
QueueValveAction = sprinkler_ns.class_("QueueValveAction", automation.Action)
SetRepeatAction = sprinkler_ns.class_("SetRepeatAction", automation.Action)
SetRunDurationAction = sprinkler_ns.class_("SetRunDurationAction", automation.Action)
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
    for sprinkler_controller_index, sprinkler_controller in enumerate(config):
        if len(sprinkler_controller[CONF_VALVES]) <= 1:
            exclusions = [
                CONF_VALVE_OPEN_DELAY,
                CONF_VALVE_OVERLAP,
                CONF_AUTO_ADVANCE_SWITCH_NAME,
                CONF_REVERSE_SWITCH_NAME,
            ]
            for config_item in exclusions:
                if config_item in sprinkler_controller:
                    raise cv.Invalid(f"Do not define {config_item} with only one valve")
            if CONF_ENABLE_SWITCH_NAME in sprinkler_controller[CONF_VALVES][0]:
                raise cv.Invalid(
                    f"Do not define {CONF_ENABLE_SWITCH_NAME} with only one valve"
                )
        else:
            requirements = [
                CONF_AUTO_ADVANCE_SWITCH_NAME,
                CONF_NAME,
            ]
            for config_item in requirements:
                if config_item not in sprinkler_controller:
                    raise cv.Invalid(
                        f"{config_item} is a required option for {sprinkler_controller_index}"
                    )

        if (
            CONF_PUMP_SWITCH_OFF_DURING_VALVE_OPEN_DELAY in sprinkler_controller
            and CONF_VALVE_OPEN_DELAY not in sprinkler_controller
        ):
            if sprinkler_controller[CONF_PUMP_SWITCH_OFF_DURING_VALVE_OPEN_DELAY]:
                raise cv.Invalid(
                    f"{CONF_VALVE_OPEN_DELAY} must be defined when {CONF_PUMP_SWITCH_OFF_DURING_VALVE_OPEN_DELAY} is enabled"
                )

        for valve in sprinkler_controller:
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

SPRINKLER_ACTION_REPEAT_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Sprinkler),
        cv.Required(CONF_REPEAT): cv.templatable(cv.positive_int),
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

SPRINKLER_ACTION_SET_RUN_DURATION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Sprinkler),
        cv.Required(CONF_RUN_DURATION): cv.templatable(cv.positive_time_period_seconds),
        cv.Required(CONF_VALVE_NUMBER): cv.templatable(cv.positive_int),
    }
)

SPRINKLER_VALVE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE_SWITCH_NAME): cv.string,
        cv.Optional(CONF_PUMP_SWITCH): cv.use_id(switch.Switch),
        cv.Required(CONF_RUN_DURATION): cv.positive_time_period_seconds,
        cv.Required(CONF_VALVE_SWITCH_NAME): cv.string,
        cv.Required(CONF_VALVE_SWITCH): cv.use_id(switch.Switch),
    }
)

SPRINKLER_CONTROLLER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Sprinkler),
        cv.GenerateID(CONF_AUTO_ADVANCE_SWITCH_ID): cv.declare_id(SprinklerSwitch),
        cv.Optional(CONF_AUTO_ADVANCE_SWITCH_NAME): cv.string,
        cv.GenerateID(CONF_MAIN_SWITCH_ID): cv.declare_id(SprinklerSwitch),
        cv.Required(CONF_NAME): cv.string,
        cv.GenerateID(CONF_REVERSE_SWITCH_ID): cv.declare_id(SprinklerSwitch),
        cv.Optional(CONF_REVERSE_SWITCH_NAME): cv.string,
        cv.Optional(CONF_MANUAL_SELECTION_DELAY): cv.positive_time_period_seconds,
        cv.Optional(CONF_REPEAT): cv.positive_int,
        cv.Optional(CONF_PUMP_SWITCH_OFF_DURING_VALVE_OPEN_DELAY): cv.boolean,
        cv.Exclusive(
            CONF_VALVE_OVERLAP, "open_delay/overlap"
        ): cv.positive_time_period_seconds,
        cv.Exclusive(
            CONF_VALVE_OPEN_DELAY, "open_delay/overlap"
        ): cv.positive_time_period_seconds,
        cv.Required(CONF_VALVES): cv.ensure_list(SPRINKLER_VALVE_SCHEMA),
    }
).extend(cv.ENTITY_BASE_SCHEMA)

CONFIG_SCHEMA = cv.All(
    cv.ensure_list(SPRINKLER_CONTROLLER_SCHEMA),
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
    "sprinkler.queue_single_valve",
    QueueValveAction,
    SPRINKLER_ACTION_SINGLE_VALVE_SCHEMA,
)
async def sprinkler_set_queued_valve_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_VALVE_NUMBER], args, cg.uint8)
    cg.add(var.set_queued_valve(template_))
    return var


@automation.register_action(
    "sprinkler.set_repeat",
    SetRepeatAction,
    SPRINKLER_ACTION_REPEAT_SCHEMA,
)
async def sprinkler_set_repeat_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_REPEAT], args, cg.float_)
    cg.add(var.set_repeat(template_))
    return var


@automation.register_action(
    "sprinkler.set_valve_run_duration",
    SetRunDurationAction,
    SPRINKLER_ACTION_SET_RUN_DURATION_SCHEMA,
)
async def sprinkler_set_valve_run_duration_to_code(
    config, action_id, template_arg, args
):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_VALVE_NUMBER], args, cg.uint8)
    cg.add(var.set_valve_number(template_))
    template_ = await cg.templatable(config[CONF_RUN_DURATION], args, cg.uint32)
    cg.add(var.set_valve_run_duration(template_))
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


def sprinkler_switch_gen(config, ss_id, ss_name):
    ss_config = config.copy()
    ss_config[CONF_ID] = ss_id
    ss_config[CONF_NAME] = ss_name
    return ss_config


async def to_code(config):
    for sprinkler_controller in config:
        var = cg.new_Pvariable(
            sprinkler_controller[CONF_ID], sprinkler_controller[CONF_NAME]
        )
        await cg.register_component(var, sprinkler_controller)
        if len(sprinkler_controller[CONF_VALVES]) > 1:
            sw_var = cg.new_Pvariable(sprinkler_controller[CONF_MAIN_SWITCH_ID])
            await cg.register_component(
                sw_var,
                sprinkler_switch_gen(
                    sprinkler_controller,
                    sprinkler_controller[CONF_MAIN_SWITCH_ID],
                    sprinkler_controller[CONF_NAME],
                ),
            )
            await switch.register_switch(
                sw_var,
                sprinkler_switch_gen(
                    sprinkler_controller,
                    sprinkler_controller[CONF_MAIN_SWITCH_ID],
                    sprinkler_controller[CONF_NAME],
                ),
            )
            sw_var = await cg.get_variable(sprinkler_controller[CONF_MAIN_SWITCH_ID])
            cg.add(var.set_controller_main_switch(sw_var))

            sw_aa_var = cg.new_Pvariable(
                sprinkler_controller[CONF_AUTO_ADVANCE_SWITCH_ID]
            )
            await cg.register_component(
                sw_aa_var,
                sprinkler_switch_gen(
                    sprinkler_controller,
                    sprinkler_controller[CONF_AUTO_ADVANCE_SWITCH_ID],
                    sprinkler_controller[CONF_AUTO_ADVANCE_SWITCH_NAME],
                ),
            )
            await switch.register_switch(
                sw_aa_var,
                sprinkler_switch_gen(
                    sprinkler_controller,
                    sprinkler_controller[CONF_AUTO_ADVANCE_SWITCH_ID],
                    sprinkler_controller[CONF_AUTO_ADVANCE_SWITCH_NAME],
                ),
            )
            sw_aa_var = await cg.get_variable(
                sprinkler_controller[CONF_AUTO_ADVANCE_SWITCH_ID]
            )
            cg.add(var.set_controller_auto_adv_switch(sw_aa_var))

            if CONF_REVERSE_SWITCH_NAME in sprinkler_controller:
                sw_rev_var = cg.new_Pvariable(
                    sprinkler_controller[CONF_REVERSE_SWITCH_ID]
                )
                await cg.register_component(
                    sw_rev_var,
                    sprinkler_switch_gen(
                        sprinkler_controller,
                        sprinkler_controller[CONF_REVERSE_SWITCH_ID],
                        sprinkler_controller[CONF_REVERSE_SWITCH_NAME],
                    ),
                )
                await switch.register_switch(
                    sw_rev_var,
                    sprinkler_switch_gen(
                        sprinkler_controller,
                        sprinkler_controller[CONF_REVERSE_SWITCH_ID],
                        sprinkler_controller[CONF_REVERSE_SWITCH_NAME],
                    ),
                )
                sw_rev_var = await cg.get_variable(
                    sprinkler_controller[CONF_REVERSE_SWITCH_ID]
                )
                cg.add(var.set_controller_reverse_switch(sw_rev_var))

        for valve in sprinkler_controller[CONF_VALVES]:
            if (
                CONF_ENABLE_SWITCH_NAME in valve
                and len(sprinkler_controller[CONF_VALVES]) > 1
            ):
                cg.add(
                    var.add_valve(
                        valve[CONF_VALVE_SWITCH_NAME], valve[CONF_ENABLE_SWITCH_NAME]
                    )
                )
            else:
                cg.add(var.add_valve(valve[CONF_VALVE_SWITCH_NAME]))

        if CONF_MANUAL_SELECTION_DELAY in sprinkler_controller:
            cg.add(
                var.set_manual_selection_delay(
                    sprinkler_controller[CONF_MANUAL_SELECTION_DELAY]
                )
            )

        if CONF_REPEAT in sprinkler_controller:
            cg.add(var.set_repeat(sprinkler_controller[CONF_REPEAT]))

        if CONF_VALVE_OVERLAP in sprinkler_controller:
            cg.add(var.set_valve_overlap(sprinkler_controller[CONF_VALVE_OVERLAP]))

        if CONF_VALVE_OPEN_DELAY in sprinkler_controller:
            cg.add(
                var.set_valve_open_delay(sprinkler_controller[CONF_VALVE_OPEN_DELAY])
            )

        if CONF_PUMP_SWITCH_OFF_DURING_VALVE_OPEN_DELAY in sprinkler_controller:
            cg.add(
                var.set_pump_switch_off_during_valve_open_delay(
                    sprinkler_controller[CONF_PUMP_SWITCH_OFF_DURING_VALVE_OPEN_DELAY]
                )
            )

    for sprinkler_controller in config:
        var = await cg.get_variable(sprinkler_controller[CONF_ID])
        for valve_index, valve in enumerate(sprinkler_controller[CONF_VALVES]):
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

    for sprinkler_controller in config:
        var = await cg.get_variable(sprinkler_controller[CONF_ID])
        for controller_to_add in config:
            if sprinkler_controller[CONF_ID] != controller_to_add[CONF_ID]:
                cg.add(
                    var.add_controller(
                        await cg.get_variable(controller_to_add[CONF_ID])
                    )
                )
