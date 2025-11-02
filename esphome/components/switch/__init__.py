from esphome import automation
from esphome.automation import Condition, maybe_simple_id
import esphome.codegen as cg
from esphome.components import mqtt, web_server
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_INVERTED,
    CONF_MQTT_ID,
    CONF_ON_STATE,
    CONF_ON_TURN_OFF,
    CONF_ON_TURN_ON,
    CONF_RESTORE_MODE,
    CONF_STATE,
    CONF_TRIGGER_ID,
    CONF_WEB_SERVER,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_OUTLET,
    DEVICE_CLASS_SWITCH,
)
from esphome.core import CORE, CoroPriority, coroutine_with_priority
from esphome.core.entity_helpers import entity_duplicate_validator, setup_entity
from esphome.cpp_generator import MockObjClass

CODEOWNERS = ["@esphome/core"]
IS_PLATFORM_COMPONENT = True
DEVICE_CLASSES = [
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_OUTLET,
    DEVICE_CLASS_SWITCH,
]

switch_ns = cg.esphome_ns.namespace("switch_")
Switch = switch_ns.class_("Switch", cg.EntityBase)
SwitchPtr = Switch.operator("ptr")

SwitchRestoreMode = switch_ns.enum("SwitchRestoreMode")

RESTORE_MODES = {
    "RESTORE_DEFAULT_OFF": SwitchRestoreMode.SWITCH_RESTORE_DEFAULT_OFF,
    "RESTORE_DEFAULT_ON": SwitchRestoreMode.SWITCH_RESTORE_DEFAULT_ON,
    "ALWAYS_OFF": SwitchRestoreMode.SWITCH_ALWAYS_OFF,
    "ALWAYS_ON": SwitchRestoreMode.SWITCH_ALWAYS_ON,
    "RESTORE_INVERTED_DEFAULT_OFF": SwitchRestoreMode.SWITCH_RESTORE_INVERTED_DEFAULT_OFF,
    "RESTORE_INVERTED_DEFAULT_ON": SwitchRestoreMode.SWITCH_RESTORE_INVERTED_DEFAULT_ON,
    "DISABLED": SwitchRestoreMode.SWITCH_RESTORE_DISABLED,
}


ControlAction = switch_ns.class_("ControlAction", automation.Action)
ToggleAction = switch_ns.class_("ToggleAction", automation.Action)
TurnOffAction = switch_ns.class_("TurnOffAction", automation.Action)
TurnOnAction = switch_ns.class_("TurnOnAction", automation.Action)
SwitchPublishAction = switch_ns.class_("SwitchPublishAction", automation.Action)

SwitchCondition = switch_ns.class_("SwitchCondition", Condition)
SwitchStateTrigger = switch_ns.class_(
    "SwitchStateTrigger", automation.Trigger.template(bool)
)
SwitchTurnOnTrigger = switch_ns.class_(
    "SwitchTurnOnTrigger", automation.Trigger.template()
)
SwitchTurnOffTrigger = switch_ns.class_(
    "SwitchTurnOffTrigger", automation.Trigger.template()
)


validate_device_class = cv.one_of(*DEVICE_CLASSES, lower=True)


_SWITCH_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTSwitchComponent),
            cv.Optional(CONF_INVERTED): cv.boolean,
            cv.Optional(CONF_RESTORE_MODE, default="ALWAYS_OFF"): cv.enum(
                RESTORE_MODES, upper=True, space="_"
            ),
            cv.Optional(CONF_ON_STATE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SwitchStateTrigger),
                }
            ),
            cv.Optional(CONF_ON_TURN_ON): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SwitchTurnOnTrigger),
                }
            ),
            cv.Optional(CONF_ON_TURN_OFF): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(SwitchTurnOffTrigger),
                }
            ),
            cv.Optional(CONF_DEVICE_CLASS): validate_device_class,
        }
    )
)


_SWITCH_SCHEMA.add_extra(entity_duplicate_validator("switch"))


def switch_schema(
    class_: MockObjClass,
    *,
    block_inverted: bool = False,
    default_restore_mode: str = cv.UNDEFINED,
    device_class: str = cv.UNDEFINED,
    entity_category: str = cv.UNDEFINED,
    icon: str = cv.UNDEFINED,
):
    schema = {cv.GenerateID(): cv.declare_id(class_)}

    for key, default, validator in [
        (CONF_DEVICE_CLASS, device_class, validate_device_class),
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
        (CONF_ICON, icon, cv.icon),
        (
            CONF_RESTORE_MODE,
            default_restore_mode,
            cv.enum(RESTORE_MODES, upper=True, space="_")
            if default_restore_mode is not cv.UNDEFINED
            else cv.UNDEFINED,
        ),
    ]:
        if default is not cv.UNDEFINED:
            schema[cv.Optional(key, default=default)] = validator

    if block_inverted:
        schema[cv.Optional(CONF_INVERTED)] = cv.invalid(
            "Inverted is not supported for this platform!"
        )

    return _SWITCH_SCHEMA.extend(schema)


async def setup_switch_core_(var, config):
    await setup_entity(var, config, "switch")

    if (inverted := config.get(CONF_INVERTED)) is not None:
        cg.add(var.set_inverted(inverted))
    for conf in config.get(CONF_ON_STATE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(bool, "x")], conf)
    for conf in config.get(CONF_ON_TURN_ON, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_TURN_OFF, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    if (mqtt_id := config.get(CONF_MQTT_ID)) is not None:
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(var, web_server_config)

    if (device_class := config.get(CONF_DEVICE_CLASS)) is not None:
        cg.add(var.set_device_class(device_class))

    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))


async def register_switch(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_switch(var))
    CORE.register_platform_component("switch", var)
    await setup_switch_core_(var, config)


async def new_switch(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_switch(var, config)
    return var


SWITCH_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Switch),
    }
)
SWITCH_CONTROL_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Switch),
        cv.Required(CONF_STATE): cv.templatable(cv.boolean),
    }
)


@automation.register_action(
    "switch.control", ControlAction, SWITCH_CONTROL_ACTION_SCHEMA
)
async def switch_control_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_STATE], args, bool)
    cg.add(var.set_state(template_))
    return var


@automation.register_action("switch.toggle", ToggleAction, SWITCH_ACTION_SCHEMA)
@automation.register_action("switch.turn_off", TurnOffAction, SWITCH_ACTION_SCHEMA)
@automation.register_action("switch.turn_on", TurnOnAction, SWITCH_ACTION_SCHEMA)
async def switch_toggle_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_condition("switch.is_on", SwitchCondition, SWITCH_ACTION_SCHEMA)
async def switch_is_on_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren, True)


@automation.register_condition("switch.is_off", SwitchCondition, SWITCH_ACTION_SCHEMA)
async def switch_is_off_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren, False)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(switch_ns.using)
