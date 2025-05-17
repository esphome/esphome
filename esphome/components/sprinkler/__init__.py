from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import number, switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INITIAL_VALUE,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_NAME,
    CONF_REPEAT,
    CONF_RESTORE_VALUE,
    CONF_RUN_DURATION,
    CONF_SET_ACTION,
    CONF_STEP,
    CONF_UNIT_OF_MEASUREMENT,
    ENTITY_CATEGORY_CONFIG,
    UNIT_MINUTE,
    UNIT_SECOND,
)

AUTO_LOAD = ["number", "switch"]
CODEOWNERS = ["@kbx81"]

CONF_AUTO_ADVANCE_SWITCH = "auto_advance_switch"
CONF_DIVIDER = "divider"
CONF_ENABLE_SWITCH = "enable_switch"
CONF_MAIN_SWITCH = "main_switch"
CONF_MANUAL_SELECTION_DELAY = "manual_selection_delay"
CONF_MULTIPLIER = "multiplier"
CONF_MULTIPLIER_NUMBER = "multiplier_number"
CONF_NEXT_PREV_IGNORE_DISABLED = "next_prev_ignore_disabled"
CONF_PUMP_OFF_SWITCH_ID = "pump_off_switch_id"
CONF_PUMP_ON_SWITCH_ID = "pump_on_switch_id"
CONF_PUMP_PULSE_DURATION = "pump_pulse_duration"
CONF_PUMP_START_PUMP_DELAY = "pump_start_pump_delay"
CONF_PUMP_START_VALVE_DELAY = "pump_start_valve_delay"
CONF_PUMP_STOP_PUMP_DELAY = "pump_stop_pump_delay"
CONF_PUMP_STOP_VALVE_DELAY = "pump_stop_valve_delay"
CONF_PUMP_SWITCH = "pump_switch"
CONF_PUMP_SWITCH_ID = "pump_switch_id"
CONF_PUMP_SWITCH_OFF_DURING_VALVE_OPEN_DELAY = "pump_switch_off_during_valve_open_delay"
CONF_QUEUE_ENABLE_SWITCH = "queue_enable_switch"
CONF_REPEAT_NUMBER = "repeat_number"
CONF_REVERSE_SWITCH = "reverse_switch"
CONF_RUN_DURATION_NUMBER = "run_duration_number"
CONF_STANDBY_SWITCH = "standby_switch"
CONF_VALVE_NUMBER = "valve_number"
CONF_VALVE_OPEN_DELAY = "valve_open_delay"
CONF_VALVE_OVERLAP = "valve_overlap"
CONF_VALVE_PULSE_DURATION = "valve_pulse_duration"
CONF_VALVE_OFF_SWITCH_ID = "valve_off_switch_id"
CONF_VALVE_ON_SWITCH_ID = "valve_on_switch_id"
CONF_VALVE_SWITCH = "valve_switch"
CONF_VALVE_SWITCH_ID = "valve_switch_id"
CONF_VALVES = "valves"

sprinkler_ns = cg.esphome_ns.namespace("sprinkler")
Sprinkler = sprinkler_ns.class_("Sprinkler", cg.Component)
SprinklerControllerNumber = sprinkler_ns.class_(
    "SprinklerControllerNumber", number.Number, cg.Component
)
SprinklerControllerSwitch = sprinkler_ns.class_(
    "SprinklerControllerSwitch", switch.Switch, cg.Component
)

SetDividerAction = sprinkler_ns.class_("SetDividerAction", automation.Action)
SetMultiplierAction = sprinkler_ns.class_("SetMultiplierAction", automation.Action)
QueueValveAction = sprinkler_ns.class_("QueueValveAction", automation.Action)
ClearQueuedValvesAction = sprinkler_ns.class_(
    "ClearQueuedValvesAction", automation.Action
)
SetRepeatAction = sprinkler_ns.class_("SetRepeatAction", automation.Action)
SetRunDurationAction = sprinkler_ns.class_("SetRunDurationAction", automation.Action)
StartFromQueueAction = sprinkler_ns.class_("StartFromQueueAction", automation.Action)
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


def validate_min_max(config):
    if config[CONF_MAX_VALUE] <= config[CONF_MIN_VALUE]:
        raise cv.Invalid(f"{CONF_MAX_VALUE} must be greater than {CONF_MIN_VALUE}")

    if (config[CONF_INITIAL_VALUE] > config[CONF_MAX_VALUE]) or (
        config[CONF_INITIAL_VALUE] < config[CONF_MIN_VALUE]
    ):
        raise cv.Invalid(
            f"{CONF_INITIAL_VALUE} must be a value between {CONF_MAX_VALUE} and {CONF_MIN_VALUE}"
        )
    return config


def validate_sprinkler(config):
    for sprinkler_controller_index, sprinkler_controller in enumerate(config):
        if len(sprinkler_controller[CONF_VALVES]) <= 1:
            exclusions = [
                CONF_VALVE_OPEN_DELAY,
                CONF_VALVE_OVERLAP,
                CONF_AUTO_ADVANCE_SWITCH,
                CONF_MAIN_SWITCH,
                CONF_REVERSE_SWITCH,
            ]
            for config_item in exclusions:
                if config_item in sprinkler_controller:
                    raise cv.Invalid(f"Do not define {config_item} with only one valve")
            if CONF_ENABLE_SWITCH in sprinkler_controller[CONF_VALVES][0]:
                raise cv.Invalid(
                    f"Do not define {CONF_ENABLE_SWITCH} with only one valve"
                )
        else:
            requirements = [
                CONF_AUTO_ADVANCE_SWITCH,
                CONF_MAIN_SWITCH,
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

        if (
            CONF_REPEAT in sprinkler_controller
            and CONF_REPEAT_NUMBER in sprinkler_controller
        ):
            raise cv.Invalid(
                f"Do not specify {CONF_REPEAT} when using {CONF_REPEAT_NUMBER}; use number component's {CONF_INITIAL_VALUE} instead"
            )

        for valve in sprinkler_controller[CONF_VALVES]:
            if (
                CONF_VALVE_OVERLAP in sprinkler_controller
                and CONF_RUN_DURATION in valve
                and valve[CONF_RUN_DURATION] <= sprinkler_controller[CONF_VALVE_OVERLAP]
            ):
                raise cv.Invalid(
                    f"{CONF_RUN_DURATION} must be greater than {CONF_VALVE_OVERLAP}"
                )
            if (
                CONF_VALVE_OPEN_DELAY in sprinkler_controller
                and CONF_RUN_DURATION in valve
                and valve[CONF_RUN_DURATION]
                <= sprinkler_controller[CONF_VALVE_OPEN_DELAY]
            ):
                raise cv.Invalid(
                    f"{CONF_RUN_DURATION} must be greater than {CONF_VALVE_OPEN_DELAY}"
                )
            if (
                CONF_PUMP_OFF_SWITCH_ID in valve and CONF_PUMP_ON_SWITCH_ID not in valve
            ) or (
                CONF_PUMP_ON_SWITCH_ID in valve and CONF_PUMP_OFF_SWITCH_ID not in valve
            ):
                raise cv.Invalid(
                    f"Both {CONF_PUMP_OFF_SWITCH_ID} and {CONF_PUMP_ON_SWITCH_ID} must be specified for latching pump configuration"
                )
            if CONF_PUMP_SWITCH_ID in valve and (
                CONF_PUMP_OFF_SWITCH_ID in valve or CONF_PUMP_ON_SWITCH_ID in valve
            ):
                raise cv.Invalid(
                    f"Do not specify {CONF_PUMP_OFF_SWITCH_ID} or {CONF_PUMP_ON_SWITCH_ID} when using {CONF_PUMP_SWITCH_ID}"
                )
            if CONF_PUMP_PULSE_DURATION not in sprinkler_controller and (
                CONF_PUMP_OFF_SWITCH_ID in valve or CONF_PUMP_ON_SWITCH_ID in valve
            ):
                raise cv.Invalid(
                    f"{CONF_PUMP_PULSE_DURATION} must be specified when using {CONF_PUMP_OFF_SWITCH_ID} and {CONF_PUMP_ON_SWITCH_ID}"
                )
            if (
                CONF_VALVE_OFF_SWITCH_ID in valve
                and CONF_VALVE_ON_SWITCH_ID not in valve
            ) or (
                CONF_VALVE_ON_SWITCH_ID in valve
                and CONF_VALVE_OFF_SWITCH_ID not in valve
            ):
                raise cv.Invalid(
                    f"Both {CONF_VALVE_OFF_SWITCH_ID} and {CONF_VALVE_ON_SWITCH_ID} must be specified for latching valve configuration"
                )
            if CONF_VALVE_SWITCH_ID in valve and (
                CONF_VALVE_OFF_SWITCH_ID in valve or CONF_VALVE_ON_SWITCH_ID in valve
            ):
                raise cv.Invalid(
                    f"Do not specify {CONF_VALVE_OFF_SWITCH_ID} or {CONF_VALVE_ON_SWITCH_ID} when using {CONF_VALVE_SWITCH_ID}"
                )
            if CONF_VALVE_PULSE_DURATION not in sprinkler_controller and (
                CONF_VALVE_OFF_SWITCH_ID in valve or CONF_VALVE_ON_SWITCH_ID in valve
            ):
                raise cv.Invalid(
                    f"{CONF_VALVE_PULSE_DURATION} must be specified when using {CONF_VALVE_OFF_SWITCH_ID} and {CONF_VALVE_ON_SWITCH_ID}"
                )
            if (
                CONF_VALVE_SWITCH_ID not in valve
                and CONF_VALVE_OFF_SWITCH_ID not in valve
                and CONF_VALVE_ON_SWITCH_ID not in valve
            ):
                raise cv.Invalid(
                    f"Either {CONF_VALVE_SWITCH_ID} or {CONF_VALVE_OFF_SWITCH_ID} and {CONF_VALVE_ON_SWITCH_ID} must be specified in valve configuration"
                )
            if CONF_RUN_DURATION not in valve and CONF_RUN_DURATION_NUMBER not in valve:
                raise cv.Invalid(
                    f"Either {CONF_RUN_DURATION} or {CONF_RUN_DURATION_NUMBER} must be specified for each valve"
                )
            if CONF_RUN_DURATION in valve and CONF_RUN_DURATION_NUMBER in valve:
                raise cv.Invalid(
                    f"Do not specify {CONF_RUN_DURATION} when using {CONF_RUN_DURATION_NUMBER}; use number component's {CONF_INITIAL_VALUE} instead"
                )
    return config


SPRINKLER_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Sprinkler),
    }
)

SPRINKLER_ACTION_REPEAT_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(Sprinkler),
        cv.Required(CONF_REPEAT): cv.templatable(cv.positive_int),
    },
    key=CONF_REPEAT,
)

SPRINKLER_ACTION_SINGLE_VALVE_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(Sprinkler),
        cv.Optional(CONF_RUN_DURATION): cv.templatable(cv.positive_time_period_seconds),
        cv.Required(CONF_VALVE_NUMBER): cv.templatable(cv.positive_int),
    },
    key=CONF_VALVE_NUMBER,
)

SPRINKLER_ACTION_SET_DIVIDER_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(Sprinkler),
        cv.Required(CONF_DIVIDER): cv.templatable(cv.positive_int),
    },
    key=CONF_DIVIDER,
)

SPRINKLER_ACTION_SET_MULTIPLIER_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(Sprinkler),
        cv.Required(CONF_MULTIPLIER): cv.templatable(cv.positive_float),
    },
    key=CONF_MULTIPLIER,
)

SPRINKLER_ACTION_SET_RUN_DURATION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Sprinkler),
        cv.Required(CONF_RUN_DURATION): cv.templatable(cv.positive_time_period_seconds),
        cv.Required(CONF_VALVE_NUMBER): cv.templatable(cv.positive_int),
    }
)

SPRINKLER_ACTION_QUEUE_VALVE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Sprinkler),
        cv.Optional(CONF_RUN_DURATION, default="0s"): cv.templatable(
            cv.positive_time_period_seconds
        ),
        cv.Required(CONF_VALVE_NUMBER): cv.templatable(cv.positive_int),
    }
)

SPRINKLER_VALVE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE_SWITCH): cv.maybe_simple_value(
            switch.switch_schema(
                SprinklerControllerSwitch,
                entity_category=ENTITY_CATEGORY_CONFIG,
                default_restore_mode="RESTORE_DEFAULT_OFF",
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_PUMP_OFF_SWITCH_ID): cv.use_id(switch.Switch),
        cv.Optional(CONF_PUMP_ON_SWITCH_ID): cv.use_id(switch.Switch),
        cv.Optional(CONF_PUMP_SWITCH_ID): cv.use_id(switch.Switch),
        cv.Optional(CONF_RUN_DURATION): cv.positive_time_period_seconds,
        cv.Optional(CONF_RUN_DURATION_NUMBER): cv.maybe_simple_value(
            number.number_schema(
                SprinklerControllerNumber, entity_category=ENTITY_CATEGORY_CONFIG
            )
            .extend(
                {
                    cv.Optional(CONF_INITIAL_VALUE, default=900): cv.positive_int,
                    cv.Optional(CONF_MAX_VALUE, default=86400): cv.positive_int,
                    cv.Optional(CONF_MIN_VALUE, default=1): cv.positive_int,
                    cv.Optional(CONF_RESTORE_VALUE, default=True): cv.boolean,
                    cv.Optional(CONF_STEP, default=1): cv.positive_int,
                    cv.Optional(CONF_SET_ACTION): automation.validate_automation(
                        single=True
                    ),
                    cv.Optional(
                        CONF_UNIT_OF_MEASUREMENT, default=UNIT_SECOND
                    ): cv.one_of(UNIT_MINUTE, UNIT_SECOND, lower="True"),
                }
            )
            .extend(cv.COMPONENT_SCHEMA),
            validate_min_max,
            key=CONF_NAME,
        ),
        cv.Required(CONF_VALVE_SWITCH): cv.maybe_simple_value(
            switch.switch_schema(SprinklerControllerSwitch),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_VALVE_OFF_SWITCH_ID): cv.use_id(switch.Switch),
        cv.Optional(CONF_VALVE_ON_SWITCH_ID): cv.use_id(switch.Switch),
        cv.Optional(CONF_VALVE_SWITCH_ID): cv.use_id(switch.Switch),
    }
)

SPRINKLER_CONTROLLER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Sprinkler),
        cv.Optional(CONF_NAME): cv.string,
        cv.Optional(CONF_AUTO_ADVANCE_SWITCH): cv.maybe_simple_value(
            switch.switch_schema(
                SprinklerControllerSwitch,
                entity_category=ENTITY_CATEGORY_CONFIG,
                default_restore_mode="RESTORE_DEFAULT_OFF",
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_MAIN_SWITCH): cv.maybe_simple_value(
            switch.switch_schema(SprinklerControllerSwitch),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_QUEUE_ENABLE_SWITCH): cv.maybe_simple_value(
            switch.switch_schema(
                SprinklerControllerSwitch,
                entity_category=ENTITY_CATEGORY_CONFIG,
                default_restore_mode="RESTORE_DEFAULT_OFF",
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_REVERSE_SWITCH): cv.maybe_simple_value(
            switch.switch_schema(
                SprinklerControllerSwitch,
                entity_category=ENTITY_CATEGORY_CONFIG,
                default_restore_mode="RESTORE_DEFAULT_OFF",
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_STANDBY_SWITCH): cv.maybe_simple_value(
            switch.switch_schema(
                SprinklerControllerSwitch,
                entity_category=ENTITY_CATEGORY_CONFIG,
                default_restore_mode="RESTORE_DEFAULT_OFF",
            ),
            key=CONF_NAME,
        ),
        cv.Optional(CONF_NEXT_PREV_IGNORE_DISABLED, default=False): cv.boolean,
        cv.Optional(CONF_MANUAL_SELECTION_DELAY): cv.positive_time_period_seconds,
        cv.Optional(CONF_MULTIPLIER_NUMBER): cv.maybe_simple_value(
            number.number_schema(
                SprinklerControllerNumber, entity_category=ENTITY_CATEGORY_CONFIG
            )
            .extend(
                {
                    cv.Optional(CONF_INITIAL_VALUE, default=1): cv.positive_float,
                    cv.Optional(CONF_MAX_VALUE, default=10): cv.positive_float,
                    cv.Optional(CONF_MIN_VALUE, default=0): cv.positive_float,
                    cv.Optional(CONF_RESTORE_VALUE, default=True): cv.boolean,
                    cv.Optional(CONF_STEP, default=0.1): cv.positive_float,
                    cv.Optional(CONF_SET_ACTION): automation.validate_automation(
                        single=True
                    ),
                }
            )
            .extend(cv.COMPONENT_SCHEMA),
            validate_min_max,
            key=CONF_NAME,
        ),
        cv.Optional(CONF_REPEAT): cv.positive_int,
        cv.Optional(CONF_REPEAT_NUMBER): cv.maybe_simple_value(
            number.number_schema(
                SprinklerControllerNumber, entity_category=ENTITY_CATEGORY_CONFIG
            )
            .extend(
                {
                    cv.Optional(CONF_INITIAL_VALUE, default=0): cv.positive_int,
                    cv.Optional(CONF_MAX_VALUE, default=10): cv.positive_int,
                    cv.Optional(CONF_MIN_VALUE, default=0): cv.positive_int,
                    cv.Optional(CONF_RESTORE_VALUE, default=True): cv.boolean,
                    cv.Optional(CONF_STEP, default=1): cv.positive_int,
                    cv.Optional(CONF_SET_ACTION): automation.validate_automation(
                        single=True
                    ),
                }
            )
            .extend(cv.COMPONENT_SCHEMA),
            validate_min_max,
            key=CONF_NAME,
        ),
        cv.Optional(CONF_PUMP_PULSE_DURATION): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_VALVE_PULSE_DURATION): cv.positive_time_period_milliseconds,
        cv.Exclusive(
            CONF_PUMP_START_PUMP_DELAY, "pump_start_xxxx_delay"
        ): cv.positive_time_period_seconds,
        cv.Exclusive(
            CONF_PUMP_STOP_PUMP_DELAY, "pump_stop_xxxx_delay"
        ): cv.positive_time_period_seconds,
        cv.Optional(CONF_PUMP_SWITCH_OFF_DURING_VALVE_OPEN_DELAY): cv.boolean,
        cv.Exclusive(
            CONF_PUMP_START_VALVE_DELAY, "pump_start_xxxx_delay"
        ): cv.positive_time_period_seconds,
        cv.Exclusive(
            CONF_PUMP_STOP_VALVE_DELAY, "pump_stop_xxxx_delay"
        ): cv.positive_time_period_seconds,
        cv.Exclusive(
            CONF_VALVE_OVERLAP, "open_delay/overlap"
        ): cv.positive_time_period_seconds,
        cv.Exclusive(
            CONF_VALVE_OPEN_DELAY, "open_delay/overlap"
        ): cv.positive_time_period_seconds,
        cv.Required(CONF_VALVES): cv.ensure_list(SPRINKLER_VALVE_SCHEMA),
    }
).extend(cv.COMPONENT_SCHEMA)


CONFIG_SCHEMA = cv.All(
    cv.ensure_list(SPRINKLER_CONTROLLER_SCHEMA),
    validate_sprinkler,
)


@automation.register_action(
    "sprinkler.set_divider",
    SetDividerAction,
    SPRINKLER_ACTION_SET_DIVIDER_SCHEMA,
)
async def sprinkler_set_divider_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_DIVIDER], args, cg.float_)
    cg.add(var.set_divider(template_))
    return var


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
    "sprinkler.queue_valve",
    QueueValveAction,
    SPRINKLER_ACTION_QUEUE_VALVE_SCHEMA,
)
async def sprinkler_set_queued_valve_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_VALVE_NUMBER], args, cg.uint8)
    cg.add(var.set_valve_number(template_))
    template_ = await cg.templatable(config[CONF_RUN_DURATION], args, cg.uint32)
    cg.add(var.set_valve_run_duration(template_))
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
    "sprinkler.start_from_queue", StartFromQueueAction, SPRINKLER_ACTION_SCHEMA
)
async def sprinkler_start_from_queue_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


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
    if CONF_RUN_DURATION in config:
        template_ = await cg.templatable(config[CONF_RUN_DURATION], args, cg.uint32)
        cg.add(var.set_valve_run_duration(template_))
    return var


@automation.register_action(
    "sprinkler.clear_queued_valves", ClearQueuedValvesAction, SPRINKLER_ACTION_SCHEMA
)
@automation.register_action(
    "sprinkler.next_valve", NextValveAction, SPRINKLER_ACTION_SCHEMA
)
@automation.register_action(
    "sprinkler.previous_valve", PreviousValveAction, SPRINKLER_ACTION_SCHEMA
)
@automation.register_action("sprinkler.pause", PauseAction, SPRINKLER_ACTION_SCHEMA)
@automation.register_action("sprinkler.resume", ResumeAction, SPRINKLER_ACTION_SCHEMA)
@automation.register_action(
    "sprinkler.resume_or_start_full_cycle", ResumeOrStartAction, SPRINKLER_ACTION_SCHEMA
)
@automation.register_action(
    "sprinkler.shutdown", ShutdownAction, SPRINKLER_ACTION_SCHEMA
)
async def sprinkler_simple_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


async def to_code(config):
    for sprinkler_controller in config:
        if len(sprinkler_controller[CONF_VALVES]) > 1:
            name = sprinkler_controller[CONF_MAIN_SWITCH][CONF_NAME]
        else:
            name = sprinkler_controller[CONF_VALVES][0][CONF_VALVE_SWITCH][CONF_NAME]
        name = sprinkler_controller.get(CONF_NAME, name)
        var = cg.new_Pvariable(sprinkler_controller[CONF_ID], name)

        await cg.register_component(var, sprinkler_controller)

        if len(sprinkler_controller[CONF_VALVES]) > 1:
            sw_var = await switch.new_switch(sprinkler_controller[CONF_MAIN_SWITCH])
            await cg.register_component(sw_var, sprinkler_controller[CONF_MAIN_SWITCH])
            cg.add(var.set_controller_main_switch(sw_var))

            sw_aa_var = await switch.new_switch(
                sprinkler_controller[CONF_AUTO_ADVANCE_SWITCH]
            )
            await cg.register_component(
                sw_aa_var, sprinkler_controller[CONF_AUTO_ADVANCE_SWITCH]
            )
            cg.add(var.set_controller_auto_adv_switch(sw_aa_var))

            if CONF_REVERSE_SWITCH in sprinkler_controller:
                sw_rev_var = await switch.new_switch(
                    sprinkler_controller[CONF_REVERSE_SWITCH]
                )
                await cg.register_component(
                    sw_rev_var, sprinkler_controller[CONF_REVERSE_SWITCH]
                )
                cg.add(var.set_controller_reverse_switch(sw_rev_var))

        if CONF_STANDBY_SWITCH in sprinkler_controller:
            sw_stb_var = await switch.new_switch(
                sprinkler_controller[CONF_STANDBY_SWITCH]
            )
            await cg.register_component(
                sw_stb_var, sprinkler_controller[CONF_STANDBY_SWITCH]
            )
            cg.add(var.set_controller_standby_switch(sw_stb_var))

        if CONF_QUEUE_ENABLE_SWITCH in sprinkler_controller:
            sw_qen_var = await switch.new_switch(
                sprinkler_controller[CONF_QUEUE_ENABLE_SWITCH]
            )
            await cg.register_component(
                sw_qen_var, sprinkler_controller[CONF_QUEUE_ENABLE_SWITCH]
            )
            cg.add(var.set_controller_queue_enable_switch(sw_qen_var))

        if CONF_MULTIPLIER_NUMBER in sprinkler_controller:
            num_mult_var = await number.new_number(
                sprinkler_controller[CONF_MULTIPLIER_NUMBER],
                min_value=sprinkler_controller[CONF_MULTIPLIER_NUMBER][CONF_MIN_VALUE],
                max_value=sprinkler_controller[CONF_MULTIPLIER_NUMBER][CONF_MAX_VALUE],
                step=sprinkler_controller[CONF_MULTIPLIER_NUMBER][CONF_STEP],
            )
            await cg.register_component(
                num_mult_var, sprinkler_controller[CONF_MULTIPLIER_NUMBER]
            )
            cg.add(
                num_mult_var.set_initial_value(
                    sprinkler_controller[CONF_MULTIPLIER_NUMBER][CONF_INITIAL_VALUE]
                )
            )
            cg.add(
                num_mult_var.set_restore_value(
                    sprinkler_controller[CONF_MULTIPLIER_NUMBER][CONF_RESTORE_VALUE]
                )
            )

            if CONF_SET_ACTION in sprinkler_controller[CONF_MULTIPLIER_NUMBER]:
                await automation.build_automation(
                    num_mult_var.get_set_trigger(),
                    [(float, "x")],
                    sprinkler_controller[CONF_MULTIPLIER_NUMBER][CONF_SET_ACTION],
                )

            cg.add(var.set_controller_multiplier_number(num_mult_var))

        if CONF_REPEAT_NUMBER in sprinkler_controller:
            num_repeat_var = await number.new_number(
                sprinkler_controller[CONF_REPEAT_NUMBER],
                min_value=sprinkler_controller[CONF_REPEAT_NUMBER][CONF_MIN_VALUE],
                max_value=sprinkler_controller[CONF_REPEAT_NUMBER][CONF_MAX_VALUE],
                step=sprinkler_controller[CONF_REPEAT_NUMBER][CONF_STEP],
            )
            await cg.register_component(
                num_repeat_var, sprinkler_controller[CONF_REPEAT_NUMBER]
            )
            cg.add(
                num_repeat_var.set_initial_value(
                    sprinkler_controller[CONF_REPEAT_NUMBER][CONF_INITIAL_VALUE]
                )
            )
            cg.add(
                num_repeat_var.set_restore_value(
                    sprinkler_controller[CONF_REPEAT_NUMBER][CONF_RESTORE_VALUE]
                )
            )

            if CONF_SET_ACTION in sprinkler_controller[CONF_REPEAT_NUMBER]:
                await automation.build_automation(
                    num_repeat_var.get_set_trigger(),
                    [(float, "x")],
                    sprinkler_controller[CONF_REPEAT_NUMBER][CONF_SET_ACTION],
                )

            cg.add(var.set_controller_repeat_number(num_repeat_var))

        for valve in sprinkler_controller[CONF_VALVES]:
            sw_valve_var = await switch.new_switch(valve[CONF_VALVE_SWITCH])
            await cg.register_component(sw_valve_var, valve[CONF_VALVE_SWITCH])

            if (
                CONF_ENABLE_SWITCH in valve
                and len(sprinkler_controller[CONF_VALVES]) > 1
            ):
                sw_en_var = await switch.new_switch(valve[CONF_ENABLE_SWITCH])
                await cg.register_component(sw_en_var, valve[CONF_ENABLE_SWITCH])

                cg.add(var.add_valve(sw_valve_var, sw_en_var))
            else:
                cg.add(var.add_valve(sw_valve_var))

        cg.add(
            var.set_next_prev_ignore_disabled_valves(
                sprinkler_controller[CONF_NEXT_PREV_IGNORE_DISABLED]
            )
        )

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

        if CONF_PUMP_START_PUMP_DELAY in sprinkler_controller:
            cg.add(
                var.set_pump_start_delay(
                    sprinkler_controller[CONF_PUMP_START_PUMP_DELAY]
                )
            )

        if CONF_PUMP_STOP_PUMP_DELAY in sprinkler_controller:
            cg.add(
                var.set_pump_stop_delay(sprinkler_controller[CONF_PUMP_STOP_PUMP_DELAY])
            )

        if CONF_PUMP_START_VALVE_DELAY in sprinkler_controller:
            cg.add(
                var.set_valve_start_delay(
                    sprinkler_controller[CONF_PUMP_START_VALVE_DELAY]
                )
            )

        if CONF_PUMP_STOP_VALVE_DELAY in sprinkler_controller:
            cg.add(
                var.set_valve_stop_delay(
                    sprinkler_controller[CONF_PUMP_STOP_VALVE_DELAY]
                )
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
            if CONF_RUN_DURATION not in valve:
                valve[CONF_RUN_DURATION] = valve[CONF_RUN_DURATION_NUMBER][
                    CONF_INITIAL_VALUE
                ]

            if CONF_VALVE_SWITCH_ID in valve:
                valve_switch = await cg.get_variable(valve[CONF_VALVE_SWITCH_ID])
                cg.add(
                    var.configure_valve_switch(
                        valve_index, valve_switch, valve[CONF_RUN_DURATION]
                    )
                )
            elif CONF_VALVE_OFF_SWITCH_ID in valve and CONF_VALVE_ON_SWITCH_ID in valve:
                valve_switch_off = await cg.get_variable(
                    valve[CONF_VALVE_OFF_SWITCH_ID]
                )
                valve_switch_on = await cg.get_variable(valve[CONF_VALVE_ON_SWITCH_ID])
                cg.add(
                    var.configure_valve_switch_pulsed(
                        valve_index,
                        valve_switch_off,
                        valve_switch_on,
                        sprinkler_controller[CONF_VALVE_PULSE_DURATION],
                        valve[CONF_RUN_DURATION],
                    )
                )

            if CONF_PUMP_SWITCH_ID in valve:
                pump = await cg.get_variable(valve[CONF_PUMP_SWITCH_ID])
                cg.add(var.configure_valve_pump_switch(valve_index, pump))
            elif CONF_PUMP_OFF_SWITCH_ID in valve and CONF_PUMP_ON_SWITCH_ID in valve:
                pump_off = await cg.get_variable(valve[CONF_PUMP_OFF_SWITCH_ID])
                pump_on = await cg.get_variable(valve[CONF_PUMP_ON_SWITCH_ID])
                cg.add(
                    var.configure_valve_pump_switch_pulsed(
                        valve_index,
                        pump_off,
                        pump_on,
                        sprinkler_controller[CONF_PUMP_PULSE_DURATION],
                    )
                )

            if CONF_RUN_DURATION_NUMBER in valve:
                num_rd_var = await number.new_number(
                    valve[CONF_RUN_DURATION_NUMBER],
                    min_value=valve[CONF_RUN_DURATION_NUMBER][CONF_MIN_VALUE],
                    max_value=valve[CONF_RUN_DURATION_NUMBER][CONF_MAX_VALUE],
                    step=valve[CONF_RUN_DURATION_NUMBER][CONF_STEP],
                )
                await cg.register_component(num_rd_var, valve[CONF_RUN_DURATION_NUMBER])

                cg.add(
                    num_rd_var.set_initial_value(
                        valve[CONF_RUN_DURATION_NUMBER][CONF_INITIAL_VALUE]
                    )
                )
                cg.add(
                    num_rd_var.set_restore_value(
                        valve[CONF_RUN_DURATION_NUMBER][CONF_RESTORE_VALUE]
                    )
                )

                if CONF_SET_ACTION in valve[CONF_RUN_DURATION_NUMBER]:
                    await automation.build_automation(
                        num_rd_var.get_set_trigger(),
                        [(float, "x")],
                        valve[CONF_RUN_DURATION_NUMBER][CONF_SET_ACTION],
                    )

                cg.add(var.configure_valve_run_duration_number(valve_index, num_rd_var))

    for sprinkler_controller in config:
        var = await cg.get_variable(sprinkler_controller[CONF_ID])
        for controller_to_add in config:
            if sprinkler_controller[CONF_ID] != controller_to_add[CONF_ID]:
                cg.add(
                    var.add_controller(
                        await cg.get_variable(controller_to_add[CONF_ID])
                    )
                )
