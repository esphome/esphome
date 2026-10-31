from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_CODE,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_MQTT_ID,
    CONF_ON_STATE,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority, entity_helpers
from esphome.core.entity_helpers import (
    entity_duplicate_validator,
    mqtt_component_class,
    queue_entity_register,
    setup_entity,
)
from esphome.cpp_generator import MockObjClass

CODEOWNERS = ["@grahambrown11", "@hwstar"]
DOMAIN = "alarm_control_panel"
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


_ALARM_CONTROL_PANEL_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(entity_helpers.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(
                mqtt_component_class("MQTTAlarmControlPanelComponent")
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
    }
)

ALARM_CONTROL_PANEL_CODE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(AlarmControlPanel),
        cv.Optional(CONF_CODE): cv.templatable(cv.string),
    }
)

ALARM_CONTROL_PANEL_CONDITION_SCHEMA = ALARM_CONTROL_PANEL_ACTION_SCHEMA


def _on_state_enter(conf_key: str, state: cg.MockObj) -> automation.CallbackAutomation:
    return automation.CallbackAutomation(
        conf_key,
        "add_on_state_callback",
        forwarder=StateEnterForwarder.template(state),
        params=[(AlarmControlPanelState, "state")],
        forward=[],
        when=f"state == {state}",
    )


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_STATE,
        "add_on_state_callback",
        forwarder=StateAnyForwarder,
        params=[(AlarmControlPanelState, "state")],
        forward=[],
    ),
    _on_state_enter(CONF_ON_TRIGGERED, AlarmControlPanelState.ACP_STATE_TRIGGERED),
    _on_state_enter(CONF_ON_ARMING, AlarmControlPanelState.ACP_STATE_ARMING),
    _on_state_enter(CONF_ON_PENDING, AlarmControlPanelState.ACP_STATE_PENDING),
    _on_state_enter(CONF_ON_ARMED_HOME, AlarmControlPanelState.ACP_STATE_ARMED_HOME),
    _on_state_enter(CONF_ON_ARMED_NIGHT, AlarmControlPanelState.ACP_STATE_ARMED_NIGHT),
    _on_state_enter(CONF_ON_ARMED_AWAY, AlarmControlPanelState.ACP_STATE_ARMED_AWAY),
    _on_state_enter(CONF_ON_DISARMED, AlarmControlPanelState.ACP_STATE_DISARMED),
    automation.CallbackAutomation(CONF_ON_CLEARED, "add_on_cleared_callback"),
    automation.CallbackAutomation(CONF_ON_CHIME, "add_on_chime_callback"),
    automation.CallbackAutomation(CONF_ON_READY, "add_on_ready_callback"),
)


@setup_entity("alarm_control_panel")
async def setup_alarm_control_panel_core_(var, config):
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)
    await entity_helpers.setup_entity_integrations(var, config)


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


# Mirrors AlarmControlPanel::arm_with_code_: arm first, set the code only when given.
for _name, _arm in (
    ("alarm_control_panel.arm_away", "arm_away()"),
    ("alarm_control_panel.arm_home", "arm_home()"),
    ("alarm_control_panel.arm_night", "arm_night()"),
    ("alarm_control_panel.disarm", "disarm()"),
):
    automation.register_apply_action(
        _name,
        ALARM_CONTROL_PANEL_CODE_ACTION_SCHEMA,
        automation.ApplyCall(_arm),
        automation.ApplyField(CONF_CODE, "set_code", cg.std_string),
        call="make_call",
    )


for _name, _call in (
    ("alarm_control_panel.pending", "pending()"),
    ("alarm_control_panel.triggered", "triggered()"),
):
    automation.register_apply_action(
        _name,
        ALARM_CONTROL_PANEL_ACTION_SCHEMA,
        automation.ApplyCall(_call),
        call="make_call",
    )


for _name in ("alarm_control_panel.ready", "alarm_control_panel.is_armed"):
    automation.register_apply_condition(
        _name, ALARM_CONTROL_PANEL_CONDITION_SCHEMA, "is_armed_pending_or_triggered()"
    )


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(alarm_control_panel_ns.using)
