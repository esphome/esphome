import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.components import binary_sensor
from esphome.core import CORE, coroutine_with_priority
from esphome.const import (
    CONF_ID,
    CONF_BINARY_SENSORS,
    CONF_INPUT,
    CONF_ON_STATE,
    CONF_TRIGGER_ID,
    CONF_CODE,
)
from esphome.cpp_helpers import setup_entity

CODEOWNERS = ["@grahambrown11"]
DEPENDENCIES = ["binary_sensor"]

CONF_CODES = "codes"
CONF_BYPASS_ARMED_HOME = "bypass_armed_home"
CONF_REQUIRES_CODE_TO_ARM = "requires_code_to_arm"
CONF_ARMING_HOME_TIME = "arming_home_time"
CONF_ARMING_AWAY_TIME = "arming_away_time"
CONF_DELAY_TIME = "delay_time"
CONF_TRIGGER_TIME = "trigger_time"
CONF_ON_TRIGGERED = "on_triggered"
CONF_ON_CLEARED = "on_cleared"

alarm_control_panel_ns = cg.esphome_ns.namespace("alarm_control_panel")
AlarmControlPanel = alarm_control_panel_ns.class_("AlarmControlPanel", cg.Component)
StateTrigger = alarm_control_panel_ns.class_(
    "StateTrigger", automation.Trigger.template()
)
TriggeredTrigger = alarm_control_panel_ns.class_(
    "TriggeredTrigger", automation.Trigger.template()
)
ClearedTrigger = alarm_control_panel_ns.class_(
    "ClearedTrigger", automation.Trigger.template()
)
ArmAwayAction = alarm_control_panel_ns.class_("ArmAwayAction", automation.Action)
ArmHomeAction = alarm_control_panel_ns.class_("ArmHomeAction", automation.Action)
DisarmAction = alarm_control_panel_ns.class_("DisarmAction", automation.Action)
PendingAction = alarm_control_panel_ns.class_("PendingAction", automation.Action)
TriggeredAction = alarm_control_panel_ns.class_("TriggeredAction", automation.Action)
AlarmControlPanelCondition = alarm_control_panel_ns.class_(
    "AlarmControlPanelCondition", automation.Condition
)

CONFIG_SCHEMA_BINARY_SENSOR = cv.maybe_simple_value(
    {
        cv.Required(CONF_INPUT): cv.use_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_BYPASS_ARMED_HOME, default=False): cv.boolean,
    },
    key=CONF_INPUT,
)

CONFIG_SCHEMA_ALARM_CONTROL_PANEL = cv.ENTITY_BASE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(AlarmControlPanel),
        cv.Optional(CONF_CODES): cv.ensure_list(cv.string_strict),
        cv.Optional(CONF_REQUIRES_CODE_TO_ARM): cv.boolean,
        cv.Optional(CONF_ARMING_HOME_TIME, "0s"): cv.positive_time_period_seconds,
        cv.Optional(CONF_ARMING_AWAY_TIME, "0s"): cv.positive_time_period_seconds,
        cv.Optional(CONF_DELAY_TIME, "0s"): cv.positive_time_period_seconds,
        cv.Optional(CONF_TRIGGER_TIME, "0s"): cv.positive_time_period_seconds,
        cv.Optional(CONF_BINARY_SENSORS): cv.ensure_list(CONFIG_SCHEMA_BINARY_SENSOR),
        cv.Optional(CONF_ON_STATE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(StateTrigger),
            }
        ),
        cv.Optional(CONF_ON_TRIGGERED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TriggeredTrigger),
            }
        ),
        cv.Optional(CONF_ON_CLEARED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ClearedTrigger),
            }
        ),
    }
)

SCHEMA_ALARM_CONTROL_PANEL_ACTION = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(AlarmControlPanel),
        cv.Optional(CONF_CODE): cv.templatable(cv.string),
    }
)

SCHEMA_ALARM_CONTROL_PANEL_CONDITION = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(AlarmControlPanel),
    }
)


def validate_config(config):
    for apanel in config:
        if CONF_REQUIRES_CODE_TO_ARM in apanel and len(apanel.get(CONF_CODES, [])) == 0:
            raise cv.Invalid(
                f"{CONF_REQUIRES_CODE_TO_ARM} not needed when {CONF_CODES} not present."
            )
    return config


CONFIG_SCHEMA = cv.All(
    cv.ensure_list(CONFIG_SCHEMA_ALARM_CONTROL_PANEL),
    validate_config,
)


async def setup_alarm_control_panel_core_(var, config):
    await setup_entity(var, config)
    if CONF_CODES in config:
        for acode in config.get(CONF_CODES, []):
            cg.add(var.add_code(acode))
        if CONF_REQUIRES_CODE_TO_ARM in config:
            cg.add(var.set_requires_code_to_arm(config[CONF_REQUIRES_CODE_TO_ARM]))
    if CONF_ARMING_HOME_TIME in config:
        cg.add(var.set_arming_home_time(config[CONF_ARMING_HOME_TIME]))
    if CONF_ARMING_AWAY_TIME in config:
        cg.add(var.set_arming_away_time(config[CONF_ARMING_AWAY_TIME]))
    if CONF_DELAY_TIME in config:
        cg.add(var.set_delay_time(config[CONF_DELAY_TIME]))
    if CONF_TRIGGER_TIME in config:
        cg.add(var.set_trigger_time(config[CONF_TRIGGER_TIME]))
    for sensor in config.get(CONF_BINARY_SENSORS, []):
        bs = await cg.get_variable(sensor[CONF_INPUT])
        cg.add(var.add_sensor(bs, sensor[CONF_BYPASS_ARMED_HOME]))
    for conf in config.get(CONF_ON_STATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_TRIGGERED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_CLEARED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


async def register_alarm_control_panel(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_alarm_control_panel(var))
    await setup_alarm_control_panel_core_(var, config)


@automation.register_action(
    "alarm_control_panel.arm_away", ArmAwayAction, SCHEMA_ALARM_CONTROL_PANEL_ACTION
)
async def alarm_action_arm_away_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_CODE in config:
        templatable_ = await cg.templatable(config[CONF_CODE], args, cg.std_string)
        cg.add(var.set_code(templatable_))
    return var


@automation.register_action(
    "alarm_control_panel.arm_home", ArmHomeAction, SCHEMA_ALARM_CONTROL_PANEL_ACTION
)
async def alarm_action_arm_home_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_CODE in config:
        templatable_ = await cg.templatable(config[CONF_CODE], args, cg.std_string)
        cg.add(var.set_code(templatable_))
    return var


@automation.register_action(
    "alarm_control_panel.disarm", DisarmAction, SCHEMA_ALARM_CONTROL_PANEL_ACTION
)
async def alarm_action_disarm_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_CODE in config:
        templatable_ = await cg.templatable(config[CONF_CODE], args, cg.std_string)
        cg.add(var.set_code(templatable_))
    return var


@automation.register_action(
    "alarm_control_panel.pending", PendingAction, SCHEMA_ALARM_CONTROL_PANEL_ACTION
)
async def alarm_action_pending_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var


@automation.register_action(
    "alarm_control_panel.triggered", TriggeredAction, SCHEMA_ALARM_CONTROL_PANEL_ACTION
)
async def alarm_action_trigger_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var


@automation.register_condition(
    "alarm_control_panel.is_armed",
    AlarmControlPanelCondition,
    SCHEMA_ALARM_CONTROL_PANEL_CONDITION,
)
async def alarm_control_panel_is_armed_to_code(
    config, condition_id, template_arg, args
):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(alarm_control_panel_ns.using)
    cg.add_define("USE_ALARM_CONTROL_PANEL")
    for apanel in config:
        var = cg.new_Pvariable(apanel[CONF_ID])
        await cg.register_component(var, apanel)
        await register_alarm_control_panel(var, apanel)
