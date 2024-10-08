from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import mqtt, web_server
import esphome.config_validation as cv
from esphome.const import (
    CONF_CODE,
    CONF_ID,
    CONF_MQTT_ID,
    CONF_ON_STATE,
    CONF_TRIGGER_ID,
    CONF_WEB_SERVER,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_helpers import setup_entity

CODEOWNERS = ["@grahambrown11", "@hwstar"]
IS_PLATFORM_COMPONENT = True

CONF_ON_TRIGGERED = "on_triggered"
CONF_ON_CLEARED = "on_cleared"
CONF_ON_ARMING = "on_arming"
CONF_ON_PENDING = "on_pending"
CONF_ON_ARMED_HOME = "on_armed_home"
CONF_ON_ARMED_NIGHT = "on_armed_night"
CONF_ON_ARMED_AWAY = "on_armed_away"
CONF_ON_DISARMED = "on_disarmed"
CONF_ON_CHIME = "on_chime"
CONF_ON_READY = "on_ready"

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
ArmingTrigger = alarm_control_panel_ns.class_(
    "ArmingTrigger", automation.Trigger.template()
)
PendingTrigger = alarm_control_panel_ns.class_(
    "PendingTrigger", automation.Trigger.template()
)
ArmedHomeTrigger = alarm_control_panel_ns.class_(
    "ArmedHomeTrigger", automation.Trigger.template()
)
ArmedNightTrigger = alarm_control_panel_ns.class_(
    "ArmedNightTrigger", automation.Trigger.template()
)
ArmedAwayTrigger = alarm_control_panel_ns.class_(
    "ArmedAwayTrigger", automation.Trigger.template()
)
DisarmedTrigger = alarm_control_panel_ns.class_(
    "DisarmedTrigger", automation.Trigger.template()
)
ChimeTrigger = alarm_control_panel_ns.class_(
    "ChimeTrigger", automation.Trigger.template()
)
ReadyTrigger = alarm_control_panel_ns.class_(
    "ReadyTrigger", automation.Trigger.template()
)

ArmAwayAction = alarm_control_panel_ns.class_("ArmAwayAction", automation.Action)
ArmHomeAction = alarm_control_panel_ns.class_("ArmHomeAction", automation.Action)
ArmNightAction = alarm_control_panel_ns.class_("ArmNightAction", automation.Action)
DisarmAction = alarm_control_panel_ns.class_("DisarmAction", automation.Action)
PendingAction = alarm_control_panel_ns.class_("PendingAction", automation.Action)
TriggeredAction = alarm_control_panel_ns.class_("TriggeredAction", automation.Action)
ChimeAction = alarm_control_panel_ns.class_("ChimeAction", automation.Action)
ReadyAction = alarm_control_panel_ns.class_("ReadyAction", automation.Action)

AlarmControlPanelCondition = alarm_control_panel_ns.class_(
    "AlarmControlPanelCondition", automation.Condition
)

ALARM_CONTROL_PANEL_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(AlarmControlPanel),
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(
                mqtt.MQTTAlarmControlPanelComponent
            ),
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
            cv.Optional(CONF_ON_ARMING): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ArmingTrigger),
                }
            ),
            cv.Optional(CONF_ON_PENDING): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(PendingTrigger),
                }
            ),
            cv.Optional(CONF_ON_ARMED_HOME): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ArmedHomeTrigger),
                }
            ),
            cv.Optional(CONF_ON_ARMED_NIGHT): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ArmedNightTrigger),
                }
            ),
            cv.Optional(CONF_ON_ARMED_AWAY): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ArmedAwayTrigger),
                }
            ),
            cv.Optional(CONF_ON_DISARMED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DisarmedTrigger),
                }
            ),
            cv.Optional(CONF_ON_CLEARED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ClearedTrigger),
                }
            ),
            cv.Optional(CONF_ON_CHIME): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ChimeTrigger),
                }
            ),
            cv.Optional(CONF_ON_READY): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ReadyTrigger),
                }
            ),
        }
    )
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
    for conf in config.get(CONF_ON_ARMING, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_PENDING, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_ARMED_HOME, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_ARMED_NIGHT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_ARMED_AWAY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_DISARMED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_CLEARED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_CHIME, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_READY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(var, web_server_config)
    if mqtt_id := config.get(CONF_MQTT_ID):
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)


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
    if code_config := config.get(CONF_CODE):
        templatable_ = await cg.templatable(code_config, args, cg.std_string)
        cg.add(var.set_code(templatable_))
    return var


@automation.register_action(
    "alarm_control_panel.arm_home", ArmHomeAction, ALARM_CONTROL_PANEL_ACTION_SCHEMA
)
async def alarm_action_arm_home_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if code_config := config.get(CONF_CODE):
        templatable_ = await cg.templatable(code_config, args, cg.std_string)
        cg.add(var.set_code(templatable_))
    return var


@automation.register_action(
    "alarm_control_panel.arm_night", ArmNightAction, ALARM_CONTROL_PANEL_ACTION_SCHEMA
)
async def alarm_action_arm_night_to_code(config, action_id, template_arg, args):
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
    if code_config := config.get(CONF_CODE):
        templatable_ = await cg.templatable(code_config, args, cg.std_string)
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


@automation.register_action(
    "alarm_control_panel.chime", ChimeAction, ALARM_CONTROL_PANEL_ACTION_SCHEMA
)
async def alarm_action_chime_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var


@automation.register_action(
    "alarm_control_panel.ready", ReadyAction, ALARM_CONTROL_PANEL_ACTION_SCHEMA
)
@automation.register_condition(
    "alarm_control_panel.ready",
    AlarmControlPanelCondition,
    ALARM_CONTROL_PANEL_CONDITION_SCHEMA,
)
async def alarm_action_ready_to_code(config, action_id, template_arg, args):
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
