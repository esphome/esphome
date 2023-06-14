import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.core import CORE, coroutine_with_priority
from esphome.const import (
    CONF_ID,
    CONF_ON_STATE,
    CONF_TRIGGER_ID,
    CONF_CODE,
)
from esphome.cpp_helpers import setup_entity

CODEOWNERS = ["@grahambrown11"]
IS_PLATFORM_COMPONENT = True

CONF_ON_TRIGGERED = "on_triggered"
CONF_ON_CLEARED = "on_cleared"

alarm_control_panel_ns = cg.esphome_ns.namespace("alarm_control_panel")
AlarmControlPanel = alarm_control_panel_ns.class_("AlarmControlPanel", cg.EntityBase)

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

ALARM_CONTROL_PANEL_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(AlarmControlPanel),
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

ALARM_CONTROL_PANEL_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(AlarmControlPanel),
        cv.Optional(CONF_CODE): cv.templatable(cv.string),
    }
)

ALARM_CONTROL_PANEL_CONDITION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(AlarmControlPanel),
    }
)


async def setup_alarm_control_panel_core_(var, config):
    await setup_entity(var, config)
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
    "alarm_control_panel.arm_away", ArmAwayAction, ALARM_CONTROL_PANEL_ACTION_SCHEMA
)
async def alarm_action_arm_away_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_CODE in config:
        templatable_ = await cg.templatable(config[CONF_CODE], args, cg.std_string)
        cg.add(var.set_code(templatable_))
    return var


@automation.register_action(
    "alarm_control_panel.arm_home", ArmHomeAction, ALARM_CONTROL_PANEL_ACTION_SCHEMA
)
async def alarm_action_arm_home_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_CODE in config:
        templatable_ = await cg.templatable(config[CONF_CODE], args, cg.std_string)
        cg.add(var.set_code(templatable_))
    return var


@automation.register_action(
    "alarm_control_panel.disarm", DisarmAction, ALARM_CONTROL_PANEL_ACTION_SCHEMA
)
async def alarm_action_disarm_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_CODE in config:
        templatable_ = await cg.templatable(config[CONF_CODE], args, cg.std_string)
        cg.add(var.set_code(templatable_))
    return var


@automation.register_action(
    "alarm_control_panel.pending", PendingAction, ALARM_CONTROL_PANEL_ACTION_SCHEMA
)
async def alarm_action_pending_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var


@automation.register_action(
    "alarm_control_panel.triggered", TriggeredAction, ALARM_CONTROL_PANEL_ACTION_SCHEMA
)
async def alarm_action_trigger_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var


@automation.register_condition(
    "alarm_control_panel.is_armed",
    AlarmControlPanelCondition,
    ALARM_CONTROL_PANEL_CONDITION_SCHEMA,
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
