import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id, Condition
from esphome.components import mqtt
from esphome.const import (
    CONF_ID,
    CONF_DEVICE_CLASS,
    CONF_STATE,
    CONF_ON_OPEN,
    CONF_POSITION,
    CONF_POSITION_COMMAND_TOPIC,
    CONF_POSITION_STATE_TOPIC,
    CONF_TILT,
    CONF_TILT_COMMAND_TOPIC,
    CONF_TILT_STATE_TOPIC,
    CONF_STOP,
    CONF_MQTT_ID,
    CONF_TRIGGER_ID,
    DEVICE_CLASS_AWNING,
    DEVICE_CLASS_BLIND,
    DEVICE_CLASS_CURTAIN,
    DEVICE_CLASS_DAMPER,
    DEVICE_CLASS_DOOR,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_GARAGE,
    DEVICE_CLASS_GATE,
    DEVICE_CLASS_SHADE,
    DEVICE_CLASS_SHUTTER,
    DEVICE_CLASS_WINDOW,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.cpp_helpers import setup_entity

IS_PLATFORM_COMPONENT = True

CODEOWNERS = ["@esphome/core"]
DEVICE_CLASSES = [
    DEVICE_CLASS_AWNING,
    DEVICE_CLASS_BLIND,
    DEVICE_CLASS_CURTAIN,
    DEVICE_CLASS_DAMPER,
    DEVICE_CLASS_DOOR,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_GARAGE,
    DEVICE_CLASS_GATE,
    DEVICE_CLASS_SHADE,
    DEVICE_CLASS_SHUTTER,
    DEVICE_CLASS_WINDOW,
]

cover_ns = cg.esphome_ns.namespace("cover")

Cover = cover_ns.class_("Cover", cg.EntityBase)

COVER_OPEN = cover_ns.COVER_OPEN
COVER_CLOSED = cover_ns.COVER_CLOSED

COVER_STATES = {
    "OPEN": COVER_OPEN,
    "CLOSED": COVER_CLOSED,
}
validate_cover_state = cv.enum(COVER_STATES, upper=True)

CoverOperation = cover_ns.enum("CoverOperation")
COVER_OPERATIONS = {
    "IDLE": CoverOperation.COVER_OPERATION_IDLE,
    "OPENING": CoverOperation.COVER_OPERATION_OPENING,
    "CLOSING": CoverOperation.COVER_OPERATION_CLOSING,
}
validate_cover_operation = cv.enum(COVER_OPERATIONS, upper=True)

# Actions
OpenAction = cover_ns.class_("OpenAction", automation.Action)
CloseAction = cover_ns.class_("CloseAction", automation.Action)
StopAction = cover_ns.class_("StopAction", automation.Action)
ToggleAction = cover_ns.class_("ToggleAction", automation.Action)
ControlAction = cover_ns.class_("ControlAction", automation.Action)
CoverPublishAction = cover_ns.class_("CoverPublishAction", automation.Action)
CoverIsOpenCondition = cover_ns.class_("CoverIsOpenCondition", Condition)
CoverIsClosedCondition = cover_ns.class_("CoverIsClosedCondition", Condition)

# Triggers
CoverOpenTrigger = cover_ns.class_("CoverOpenTrigger", automation.Trigger.template())
CoverClosedTrigger = cover_ns.class_(
    "CoverClosedTrigger", automation.Trigger.template()
)

CONF_ON_CLOSED = "on_closed"

COVER_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA).extend(
    {
        cv.GenerateID(): cv.declare_id(Cover),
        cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTCoverComponent),
        cv.Optional(CONF_DEVICE_CLASS): cv.one_of(*DEVICE_CLASSES, lower=True),
        cv.Optional(CONF_POSITION_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.subscribe_topic
        ),
        cv.Optional(CONF_POSITION_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.subscribe_topic
        ),
        cv.Optional(CONF_TILT_COMMAND_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.subscribe_topic
        ),
        cv.Optional(CONF_TILT_STATE_TOPIC): cv.All(
            cv.requires_component("mqtt"), cv.subscribe_topic
        ),
        cv.Optional(CONF_ON_OPEN): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CoverOpenTrigger),
            }
        ),
        cv.Optional(CONF_ON_CLOSED): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CoverClosedTrigger),
            }
        ),
    }
)


async def setup_cover_core_(var, config):
    await setup_entity(var, config)

    if CONF_DEVICE_CLASS in config:
        cg.add(var.set_device_class(config[CONF_DEVICE_CLASS]))

    for conf in config.get(CONF_ON_OPEN, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_CLOSED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    if CONF_MQTT_ID in config:
        mqtt_ = cg.new_Pvariable(config[CONF_MQTT_ID], var)
        await mqtt.register_mqtt_component(mqtt_, config)

        if CONF_POSITION_STATE_TOPIC in config:
            cg.add(
                mqtt_.set_custom_position_state_topic(config[CONF_POSITION_STATE_TOPIC])
            )
        if CONF_POSITION_COMMAND_TOPIC in config:
            cg.add(
                mqtt_.set_custom_position_command_topic(
                    config[CONF_POSITION_COMMAND_TOPIC]
                )
            )
        if CONF_TILT_STATE_TOPIC in config:
            cg.add(mqtt_.set_custom_tilt_state_topic(config[CONF_TILT_STATE_TOPIC]))
        if CONF_TILT_COMMAND_TOPIC in config:
            cg.add(mqtt_.set_custom_tilt_command_topic(config[CONF_TILT_COMMAND_TOPIC]))


async def register_cover(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_cover(var))
    await setup_cover_core_(var, config)


COVER_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Cover),
    }
)


@automation.register_action("cover.open", OpenAction, COVER_ACTION_SCHEMA)
async def cover_open_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("cover.close", CloseAction, COVER_ACTION_SCHEMA)
async def cover_close_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("cover.stop", StopAction, COVER_ACTION_SCHEMA)
async def cover_stop_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action("cover.toggle", ToggleAction, COVER_ACTION_SCHEMA)
def cover_toggle_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, paren)


COVER_CONTROL_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Cover),
        cv.Optional(CONF_STOP): cv.templatable(cv.boolean),
        cv.Exclusive(CONF_STATE, "pos"): cv.templatable(validate_cover_state),
        cv.Exclusive(CONF_POSITION, "pos"): cv.templatable(cv.percentage),
        cv.Optional(CONF_TILT): cv.templatable(cv.percentage),
    }
)


@automation.register_action("cover.control", ControlAction, COVER_CONTROL_ACTION_SCHEMA)
async def cover_control_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    if CONF_STOP in config:
        template_ = await cg.templatable(config[CONF_STOP], args, bool)
        cg.add(var.set_stop(template_))
    if CONF_STATE in config:
        template_ = await cg.templatable(config[CONF_STATE], args, float)
        cg.add(var.set_position(template_))
    if CONF_POSITION in config:
        template_ = await cg.templatable(config[CONF_POSITION], args, float)
        cg.add(var.set_position(template_))
    if CONF_TILT in config:
        template_ = await cg.templatable(config[CONF_TILT], args, float)
        cg.add(var.set_tilt(template_))
    return var


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_COVER")
    cg.add_global(cover_ns.using)
