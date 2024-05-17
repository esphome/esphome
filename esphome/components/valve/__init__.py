import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id, Condition
from esphome.components import mqtt
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ID,
    CONF_MQTT_ID,
    CONF_ON_OPEN,
    CONF_POSITION,
    CONF_POSITION_COMMAND_TOPIC,
    CONF_POSITION_STATE_TOPIC,
    CONF_STATE,
    CONF_STOP,
    CONF_TRIGGER_ID,
    DEVICE_CLASS_GAS,
    DEVICE_CLASS_WATER,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_helpers import setup_entity

IS_PLATFORM_COMPONENT = True

CODEOWNERS = ["@esphome/core"]

DEVICE_CLASSES = [
    DEVICE_CLASS_GAS,
    DEVICE_CLASS_WATER,
]

valve_ns = cg.esphome_ns.namespace("valve")

Valve = valve_ns.class_("Valve", cg.EntityBase)

VALVE_OPEN = valve_ns.VALVE_OPEN
VALVE_CLOSED = valve_ns.VALVE_CLOSED

VALVE_STATES = {
    "OPEN": VALVE_OPEN,
    "CLOSED": VALVE_CLOSED,
}
validate_valve_state = cv.enum(VALVE_STATES, upper=True)

ValveOperation = valve_ns.enum("ValveOperation")
VALVE_OPERATIONS = {
    "IDLE": ValveOperation.VALVE_OPERATION_IDLE,
    "OPENING": ValveOperation.VALVE_OPERATION_OPENING,
    "CLOSING": ValveOperation.VALVE_OPERATION_CLOSING,
}
validate_valve_operation = cv.enum(VALVE_OPERATIONS, upper=True)

# Actions
OpenAction = valve_ns.class_("OpenAction", automation.Action)
CloseAction = valve_ns.class_("CloseAction", automation.Action)
StopAction = valve_ns.class_("StopAction", automation.Action)
ToggleAction = valve_ns.class_("ToggleAction", automation.Action)
ControlAction = valve_ns.class_("ControlAction", automation.Action)
ValvePublishAction = valve_ns.class_("ValvePublishAction", automation.Action)
ValveIsOpenCondition = valve_ns.class_("ValveIsOpenCondition", Condition)
ValveIsClosedCondition = valve_ns.class_("ValveIsClosedCondition", Condition)

# Triggers
ValveOpenTrigger = valve_ns.class_("ValveOpenTrigger", automation.Trigger.template())
ValveClosedTrigger = valve_ns.class_(
    "ValveClosedTrigger", automation.Trigger.template()
)

CONF_ON_CLOSED = "on_closed"

VALVE_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA).extend(
    {
        cv.GenerateID(): cv.declare_id(Valve),
        cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTValveComponent),
        cv.Optional(CONF_DEVICE_CLASS): cv.one_of(*DEVICE_CLASSES, lower=True),
        cv.Optional(CONF_POSITION_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.subscribe_topic
        ),
        cv.Optional(CONF_POSITION_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.subscribe_topic
        ),
        cv.Optional(CONF_ON_OPEN): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ValveOpenTrigger),
            }
        ),
        cv.Optional(CONF_ON_CLOSED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ValveClosedTrigger),
            }
        ),
    }
)


async def setup_valve_core_(var, config):
    await setup_entity(var, config)

    if device_class_config := config.get(CONF_DEVICE_CLASS):
        cg.add(var.set_device_class(device_class_config))

    for conf in config.get(CONF_ON_OPEN, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_CLOSED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    if mqtt_id_config := config.get(CONF_MQTT_ID):
        mqtt_ = cg.new_Pvariable(mqtt_id_config, var)
        await mqtt.register_mqtt_component(mqtt_, config)

        if position_state_topic_config := config.get(CONF_POSITION_STATE_TOPIC):
            cg.add(mqtt_.set_custom_position_state_topic(position_state_topic_config))
        if position_command_topic_config := config.get(CONF_POSITION_COMMAND_TOPIC):
            cg.add(
                mqtt_.set_custom_position_command_topic(position_command_topic_config)
            )


async def register_valve(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_valve(var))
    await setup_valve_core_(var, config)


async def new_valve(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_valve(var, config)
    return var


VALVE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Valve),
    }
)


@automation.register_action("valve.open", OpenAction, VALVE_ACTION_SCHEMA)
async def valve_open_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("valve.close", CloseAction, VALVE_ACTION_SCHEMA)
async def valve_close_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("valve.stop", StopAction, VALVE_ACTION_SCHEMA)
async def valve_stop_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("valve.toggle", ToggleAction, VALVE_ACTION_SCHEMA)
def valve_toggle_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, paren)


VALVE_CONTROL_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Valve),
        cv.Optional(CONF_STOP): cv.templatable(cv.boolean),
        cv.Exclusive(CONF_STATE, "pos"): cv.templatable(validate_valve_state),
        cv.Exclusive(CONF_POSITION, "pos"): cv.templatable(cv.percentage),
    }
)


@automation.register_action("valve.control", ControlAction, VALVE_CONTROL_ACTION_SCHEMA)
async def valve_control_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if stop_config := config.get(CONF_STOP):
        template_ = await cg.templatable(stop_config, args, bool)
        cg.add(var.set_stop(template_))
    if state_config := config.get(CONF_STATE):
        template_ = await cg.templatable(state_config, args, float)
        cg.add(var.set_position(template_))
    if (position_config := config.get(CONF_POSITION)) is not None:
        template_ = await cg.templatable(position_config, args, float)
        cg.add(var.set_position(template_))
    return var


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_VALVE")
    cg.add_global(valve_ns.using)
