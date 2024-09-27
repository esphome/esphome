from esphome import automation
from esphome.automation import Condition, maybe_simple_id
import esphome.codegen as cg
from esphome.components import mqtt, web_server
import esphome.config_validation as cv
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
    CONF_TILT,
    CONF_TILT_COMMAND_TOPIC,
    CONF_TILT_STATE_TOPIC,
    CONF_TRIGGER_ID,
    CONF_WEB_SERVER_ID,
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

COVER_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
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
)


async def setup_cover_core_(var, config):
    await setup_entity(var, config)

    if (device_class := config.get(CONF_DEVICE_CLASS)) is not None:
        cg.add(var.set_device_class(device_class))

    for conf in config.get(CONF_ON_OPEN, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_CLOSED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    if (webserver_id := config.get(CONF_WEB_SERVER_ID)) is not None:
        web_server_ = await cg.get_variable(webserver_id)
        web_server.add_entity_to_sorting_list(web_server_, var, config)

    if (mqtt_id := config.get(CONF_MQTT_ID)) is not None:
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

        if (position_state_topic := config.get(CONF_POSITION_STATE_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_position_state_topic(position_state_topic))
        if (
            position_command_topic := config.get(CONF_POSITION_COMMAND_TOPIC)
        ) is not None:
            cg.add(mqtt_.set_custom_position_command_topic(position_command_topic))
        if (tilt_state_topic := config.get(CONF_TILT_STATE_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_tilt_state_topic(tilt_state_topic))
        if (tilt_command_topic := config.get(CONF_TILT_COMMAND_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_tilt_command_topic(tilt_command_topic))


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
    if (stop := config.get(CONF_STOP)) is not None:
        template_ = await cg.templatable(stop, args, bool)
        cg.add(var.set_stop(template_))
    if (state := config.get(CONF_STATE)) is not None:
        template_ = await cg.templatable(state, args, float)
        cg.add(var.set_position(template_))
    if (position := config.get(CONF_POSITION)) is not None:
        template_ = await cg.templatable(position, args, float)
        cg.add(var.set_position(template_))
    if (tilt := config.get(CONF_TILT)) is not None:
        template_ = await cg.templatable(tilt, args, float)
        cg.add(var.set_tilt(template_))
    return var


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_define("USE_COVER")
    cg.add_global(cover_ns.using)
