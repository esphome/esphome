import logging

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
    CONF_MQTT_ID,
    CONF_MQTT_JSON_STATE_PAYLOAD,
    CONF_ON_IDLE,
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
    CONF_WEB_SERVER,
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
from esphome.core import CORE, ID, CoroPriority, coroutine_with_priority
from esphome.core.entity_helpers import entity_duplicate_validator, setup_entity
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType, TemplateArgsType

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

_LOGGER = logging.getLogger(__name__)

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
CoverOpenedTrigger = cover_ns.class_(
    "CoverOpenedTrigger", automation.Trigger.template()
)
CoverClosedTrigger = cover_ns.class_(
    "CoverClosedTrigger", automation.Trigger.template()
)
CoverTrigger = cover_ns.class_("CoverTrigger", automation.Trigger.template())

# Cover-specific constants
CONF_ON_CLOSED = "on_closed"
CONF_ON_OPENED = "on_opened"
CONF_ON_OPENING = "on_opening"
CONF_ON_CLOSING = "on_closing"

TRIGGERS = {
    CONF_ON_OPEN: CoverOpenedTrigger,  # Deprecated, use on_opened
    CONF_ON_OPENED: CoverOpenedTrigger,
    CONF_ON_CLOSED: CoverClosedTrigger,
    CONF_ON_CLOSING: CoverTrigger.template(CoverOperation.COVER_OPERATION_CLOSING),
    CONF_ON_OPENING: CoverTrigger.template(CoverOperation.COVER_OPERATION_OPENING),
    CONF_ON_IDLE: CoverTrigger.template(CoverOperation.COVER_OPERATION_IDLE),
}


_COVER_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTCoverComponent),
            cv.Optional(CONF_MQTT_JSON_STATE_PAYLOAD): cv.All(
                cv.requires_component("mqtt"), cv.boolean
            ),
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
            **{
                cv.Optional(conf): automation.validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(trigger_class),
                    }
                )
                for conf, trigger_class in TRIGGERS.items()
            },
        }
    )
)


_COVER_SCHEMA.add_extra(entity_duplicate_validator("cover"))


def _validate_mqtt_state_topics(config):
    if config.get(CONF_MQTT_JSON_STATE_PAYLOAD):
        if CONF_POSITION_STATE_TOPIC in config:
            raise cv.Invalid(
                f"'{CONF_POSITION_STATE_TOPIC}' cannot be used with '{CONF_MQTT_JSON_STATE_PAYLOAD}: true'"
            )
        if CONF_TILT_STATE_TOPIC in config:
            raise cv.Invalid(
                f"'{CONF_TILT_STATE_TOPIC}' cannot be used with '{CONF_MQTT_JSON_STATE_PAYLOAD}: true'"
            )
    return config


_COVER_SCHEMA.add_extra(_validate_mqtt_state_topics)


def cover_schema(
    class_: MockObjClass,
    *,
    device_class: str = cv.UNDEFINED,
    entity_category: str = cv.UNDEFINED,
    icon: str = cv.UNDEFINED,
) -> cv.Schema:
    schema = {
        cv.GenerateID(): cv.declare_id(class_),
    }

    for key, default, validator in [
        (CONF_DEVICE_CLASS, device_class, cv.one_of(*DEVICE_CLASSES, lower=True)),
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
        (CONF_ICON, icon, cv.icon),
    ]:
        if default is not cv.UNDEFINED:
            schema[cv.Optional(key, default=default)] = validator

    return _COVER_SCHEMA.extend(schema)


async def setup_cover_core_(var, config):
    await setup_entity(var, config, "cover")

    if (device_class := config.get(CONF_DEVICE_CLASS)) is not None:
        cg.add(var.set_device_class(device_class))

    if CONF_ON_OPEN in config:
        _LOGGER.warning(
            "'on_open' is deprecated, use 'on_opened'. Will be removed in 2026.8.0"
        )
    for trigger_conf in TRIGGERS:
        for conf in config.get(trigger_conf, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
            await automation.build_automation(trigger, [], conf)

    if (mqtt_id := config.get(CONF_MQTT_ID)) is not None:
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

        if (position_state_topic := config.get(CONF_POSITION_STATE_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_position_state_topic(position_state_topic))
        if (
            position_command_topic := config.get(CONF_POSITION_COMMAND_TOPIC)
        ) is not None:
            cg.add(mqtt_.set_custom_position_command_topic(position_command_topic))
        if config.get(CONF_MQTT_JSON_STATE_PAYLOAD):
            cg.add_define("USE_MQTT_COVER_JSON")
            cg.add(mqtt_.set_use_json_format(True))
        if (tilt_state_topic := config.get(CONF_TILT_STATE_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_tilt_state_topic(tilt_state_topic))
        if (tilt_command_topic := config.get(CONF_TILT_COMMAND_TOPIC)) is not None:
            cg.add(mqtt_.set_custom_tilt_command_topic(tilt_command_topic))

    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(var, web_server_config)


async def register_cover(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_cover(var))
    CORE.register_platform_component("cover", var)
    await setup_cover_core_(var, config)


async def new_cover(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_cover(var, config)
    return var


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
async def cover_toggle_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


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


COVER_CONDITION_SCHEMA = cv.maybe_simple_value(
    {cv.Required(CONF_ID): cv.use_id(Cover)}, key=CONF_ID
)


async def cover_condition_to_code(
    config: ConfigType, condition_id: ID, template_arg: MockObj, args: TemplateArgsType
) -> MockObj:
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)


automation.register_condition(
    "cover.is_open", CoverIsOpenCondition, COVER_CONDITION_SCHEMA
)(cover_condition_to_code)
automation.register_condition(
    "cover.is_closed", CoverIsClosedCondition, COVER_CONDITION_SCHEMA
)(cover_condition_to_code)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(cover_ns.using)
