from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import mqtt, web_server
import esphome.config_validation as cv
from esphome.const import (
    CONF_CODE,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_MQTT_ID,
    CONF_ON_STATE,
    CONF_WEB_SERVER,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.core.entity_helpers import (
    entity_duplicate_validator,
    queue_entity_register,
    setup_entity,
)
from esphome.cpp_generator import MockObjClass

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

StateAnyForwarder = alarm_control_panel_ns.class_("StateAnyForwarder")
StateEnterForwarder = alarm_control_panel_ns.class_("StateEnterForwarder")
AlarmControlPanelState = alarm_control_panel_ns.enum("AlarmControlPanelState")

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

_ALARM_CONTROL_PANEL_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(
                mqtt.MQTTAlarmControlPanelComponent
            ),
            cv.Optional(CONF_ON_STATE): automation.validate_automation({}),
            cv.Optional(CONF_ON_TRIGGERED): automation.validate_automation({}),
            cv.Optional(CONF_ON_ARMING): automation.validate_automation({}),
            cv.Optional(CONF_ON_PENDING): automation.validate_automation({}),
            cv.Optional(CONF_ON_ARMED_HOME): automation.validate_automation({}),
            cv.Optional(CONF_ON_ARMED_NIGHT): automation.validate_automation({}),
            cv.Optional(CONF_ON_ARMED_AWAY): automation.validate_automation({}),
            cv.Optional(CONF_ON_DISARMED): automation.validate_automation({}),
            cv.Optional(CONF_ON_CLEARED): automation.validate_automation({}),
            cv.Optional(CONF_ON_CHIME): automation.validate_automation({}),
            cv.Optional(CONF_ON_READY): automation.validate_automation({}),
        }
    )
)


_ALARM_CONTROL_PANEL_SCHEMA.add_extra(entity_duplicate_validator("alarm_control_panel"))


def alarm_control_panel_schema(
    class_: MockObjClass,
    *,
    entity_category: str = cv.UNDEFINED,
    icon: str = cv.UNDEFINED,
) -> cv.Schema:
    schema = {
        cv.GenerateID(): cv.declare_id(class_),
    }

    for key, default, validator in [
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
        (CONF_ICON, icon, cv.icon),
    ]:
        if default is not cv.UNDEFINED:
            schema[cv.Optional(key, default=default)] = validator

    return _ALARM_CONTROL_PANEL_SCHEMA.extend(schema)


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


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_STATE, "add_on_state_callback", forwarder=StateAnyForwarder
    ),
    automation.CallbackAutomation(
        CONF_ON_TRIGGERED,
        "add_on_state_callback",
        forwarder=StateEnterForwarder.template(
            AlarmControlPanelState.ACP_STATE_TRIGGERED
        ),
    ),
    automation.CallbackAutomation(
        CONF_ON_ARMING,
        "add_on_state_callback",
        forwarder=StateEnterForwarder.template(AlarmControlPanelState.ACP_STATE_ARMING),
    ),
    automation.CallbackAutomation(
        CONF_ON_PENDING,
        "add_on_state_callback",
        forwarder=StateEnterForwarder.template(
            AlarmControlPanelState.ACP_STATE_PENDING
        ),
    ),
    automation.CallbackAutomation(
        CONF_ON_ARMED_HOME,
        "add_on_state_callback",
        forwarder=StateEnterForwarder.template(
            AlarmControlPanelState.ACP_STATE_ARMED_HOME
        ),
    ),
    automation.CallbackAutomation(
        CONF_ON_ARMED_NIGHT,
        "add_on_state_callback",
        forwarder=StateEnterForwarder.template(
            AlarmControlPanelState.ACP_STATE_ARMED_NIGHT
        ),
    ),
    automation.CallbackAutomation(
        CONF_ON_ARMED_AWAY,
        "add_on_state_callback",
        forwarder=StateEnterForwarder.template(
            AlarmControlPanelState.ACP_STATE_ARMED_AWAY
        ),
    ),
    automation.CallbackAutomation(
        CONF_ON_DISARMED,
        "add_on_state_callback",
        forwarder=StateEnterForwarder.template(
            AlarmControlPanelState.ACP_STATE_DISARMED
        ),
    ),
    automation.CallbackAutomation(CONF_ON_CLEARED, "add_on_cleared_callback"),
    automation.CallbackAutomation(CONF_ON_CHIME, "add_on_chime_callback"),
    automation.CallbackAutomation(CONF_ON_READY, "add_on_ready_callback"),
)


@setup_entity("alarm_control_panel")
async def setup_alarm_control_panel_core_(var, config):
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)
    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(var, web_server_config)
    if mqtt_id := config.get(CONF_MQTT_ID):
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)


async def register_alarm_control_panel(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    queue_entity_register("alarm_control_panel", config)
    CORE.register_platform_component("alarm_control_panel", var)
    await setup_alarm_control_panel_core_(var, config)


async def new_alarm_control_panel(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_alarm_control_panel(var, config)
    return var


@automation.register_action(
    "alarm_control_panel.arm_away",
    ArmAwayAction,
    ALARM_CONTROL_PANEL_ACTION_SCHEMA,
    synchronous=True,
)
async def alarm_action_arm_away_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if code_config := config.get(CONF_CODE):
        templatable_ = await cg.templatable(code_config, args, cg.std_string)
        cg.add(var.set_code(templatable_))
    return var


@automation.register_action(
    "alarm_control_panel.arm_home",
    ArmHomeAction,
    ALARM_CONTROL_PANEL_ACTION_SCHEMA,
    synchronous=True,
)
async def alarm_action_arm_home_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if code_config := config.get(CONF_CODE):
        templatable_ = await cg.templatable(code_config, args, cg.std_string)
        cg.add(var.set_code(templatable_))
    return var


@automation.register_action(
    "alarm_control_panel.arm_night",
    ArmNightAction,
    ALARM_CONTROL_PANEL_ACTION_SCHEMA,
    synchronous=True,
)
async def alarm_action_arm_night_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_CODE in config:
        templatable_ = await cg.templatable(config[CONF_CODE], args, cg.std_string)
        cg.add(var.set_code(templatable_))
    return var


@automation.register_action(
    "alarm_control_panel.disarm",
    DisarmAction,
    ALARM_CONTROL_PANEL_ACTION_SCHEMA,
    synchronous=True,
)
async def alarm_action_disarm_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if code_config := config.get(CONF_CODE):
        templatable_ = await cg.templatable(code_config, args, cg.std_string)
        cg.add(var.set_code(templatable_))
    return var


@automation.register_action(
    "alarm_control_panel.pending",
    PendingAction,
    ALARM_CONTROL_PANEL_ACTION_SCHEMA,
    synchronous=True,
)
async def alarm_action_pending_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "alarm_control_panel.triggered",
    TriggeredAction,
    ALARM_CONTROL_PANEL_ACTION_SCHEMA,
    synchronous=True,
)
async def alarm_action_trigger_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "alarm_control_panel.chime",
    ChimeAction,
    ALARM_CONTROL_PANEL_ACTION_SCHEMA,
    synchronous=True,
)
async def alarm_action_chime_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "alarm_control_panel.ready",
    ReadyAction,
    ALARM_CONTROL_PANEL_ACTION_SCHEMA,
    synchronous=True,
)
@automation.register_condition(
    "alarm_control_panel.ready",
    AlarmControlPanelCondition,
    ALARM_CONTROL_PANEL_CONDITION_SCHEMA,
)
async def alarm_action_ready_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


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


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(alarm_control_panel_ns.using)
