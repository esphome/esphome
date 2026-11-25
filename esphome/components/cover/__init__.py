from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
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
from esphome.core import CORE, CoroPriority, coroutine_with_priority, entity_helpers
from esphome.core.entity_helpers import (
    entity_duplicate_validator,
    mqtt_component_class,
    queue_entity_register,
    setup_device_class,
    setup_entity,
)
from esphome.cpp_generator import (
    AssignmentExpression,
    MockObj,
    MockObjClass,
    Statement,
    VariableDeclarationExpression,
    statement,
)
from esphome.types import ConfigType, SafeExpType

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
DOMAIN = "cover"

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
COVER_OPERATIONS = CoverOperation.options(
    "IDLE", "OPENING", "CLOSING", prefix="COVER_OPERATION_"
)
validate_cover_operation = cv.enum(COVER_OPERATIONS, upper=True)

# Cover-specific constants
CONF_ON_CLOSED = "on_closed"
CONF_ON_OPENED = "on_opened"
CONF_ON_OPENING = "on_opening"
CONF_ON_CLOSING = "on_closing"
CONF_STOP_TILT = "stop_tilt"

# The cover member and value each trigger fires on entering.
_STATE_TRIGGERS = {
    CONF_ON_OPENED: ("position", COVER_OPEN),
    CONF_ON_CLOSED: ("position", COVER_CLOSED),
    CONF_ON_CLOSING: ("current_operation", CoverOperation.COVER_OPERATION_CLOSING),
    CONF_ON_OPENING: ("current_operation", CoverOperation.COVER_OPERATION_OPENING),
    CONF_ON_IDLE: ("current_operation", CoverOperation.COVER_OPERATION_IDLE),
}


_COVER_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(entity_helpers.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(
                mqtt_component_class("MQTTCoverComponent")
            ),
            cv.Optional(CONF_MQTT_JSON_STATE_PAYLOAD): cv.All(
                cv.requires_component("mqtt"), cv.boolean
            ),
            cv.Optional(
                CONF_DEVICE_CLASS, visibility=cv.Visibility.ADVANCED
            ): cv.one_of(*DEVICE_CLASSES, lower=True),
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
                cv.Optional(conf): automation.validate_automation({})
                for conf in _STATE_TRIGGERS
            },
            # Removed in 2026.12.0 - kept to provide helpful error message
            cv.Optional(CONF_ON_OPEN): cv.invalid(
                "The 'on_open' trigger was removed in ESPHome 2026.12.0, "
                "use 'on_opened' instead."
            ),
        }
    )
)


_COVER_SCHEMA.add_extra(entity_duplicate_validator("cover"))


def _validate_mqtt_state_topics(config: ConfigType) -> ConfigType:
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


@setup_entity("cover")
async def setup_cover_core_(var: MockObj, config: ConfigType) -> None:
    setup_device_class(config)

    await _build_state_triggers(var, config)

    mqtt_ = await entity_helpers.setup_entity_integrations(var, config)
    if mqtt_ is not None:
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


async def _build_state_triggers(var: MockObj, config: ConfigType) -> None:
    """Register one state callback that runs each state trigger once on entering its state.

    Each automation keeps its own flag of whether it saw its state on the last publish, so a
    publish from an action is seen the same way as with one callback per trigger.
    """
    parent = automation.parent_ref(var)
    seen: list[Statement] = []
    checks: list[Statement] = []
    for conf_key, (member, value) in _STATE_TRIGGERS.items():
        for conf in config.get(conf_key, []):
            flag = MockObj(f"seen_{len(seen)}")
            declaration = VariableDeclarationExpression(cg.bool_, "", flag, static=True)
            seen.append(statement(AssignmentExpression(None, None, declaration, False)))
            entered = cover_ns.state_entered(flag, getattr(parent, member) == value)
            target = await automation.build_trigger_callback([], conf, [])
            run = statement(MockObj(target)())
            checks.append(cg.RawStatement(f"if ({entered})\n  {run}"))
    if not checks:
        return
    callback = automation.apply_static_function(
        config[CONF_ID], cg.void, [], seen + checks, suffix="state_triggers"
    )
    cg.add(var.add_on_state_callback(callback))


async def register_cover(var: MockObj, config: ConfigType) -> None:
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    queue_entity_register("cover", config)
    CORE.register_platform_component("cover", var)
    await setup_cover_core_(var, config)


async def new_cover(config: ConfigType, *args: SafeExpType) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_cover(var, config)
    return var


COVER_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Cover),
    }
)


for _name, _command in (
    ("cover.open", "set_command_open()"),
    ("cover.close", "set_command_close()"),
    ("cover.stop", "set_command_stop()"),
    ("cover.stop_tilt", "set_command_stop_tilt()"),
    ("cover.toggle", "set_command_toggle()"),
):
    automation.register_apply_action(
        _name, COVER_ACTION_SCHEMA, automation.ApplyCall(_command), call="make_call"
    )


COVER_CONTROL_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Cover),
        cv.Optional(CONF_STOP): cv.templatable(cv.boolean),
        cv.Optional(CONF_STOP_TILT): cv.templatable(cv.boolean),
        cv.Exclusive(CONF_STATE, "pos"): cv.templatable(validate_cover_state),
        cv.Exclusive(CONF_POSITION, "pos"): cv.templatable(cv.percentage),
        cv.Optional(CONF_TILT): cv.templatable(cv.percentage),
    }
)


# CONF_STATE and CONF_POSITION are cv.Exclusive in the schema, so at most
# one is present and both dispatch to set_position.
automation.register_apply_action(
    "cover.control",
    COVER_CONTROL_ACTION_SCHEMA,
    automation.ApplyField(CONF_STOP, "set_stop", cg.bool_),
    automation.ApplyField(CONF_STOP_TILT, "set_stop_tilt", cg.bool_),
    automation.ApplyField(CONF_STATE, "set_position", cg.float_),
    automation.ApplyField(CONF_POSITION, "set_position", cg.float_),
    automation.ApplyField(CONF_TILT, "set_tilt", cg.float_),
    call="make_call",
)


COVER_CONDITION_SCHEMA = cv.maybe_simple_value(
    {cv.Required(CONF_ID): cv.use_id(Cover)}, key=CONF_ID
)


automation.register_apply_condition(
    "cover.is_open", COVER_CONDITION_SCHEMA, f"position == {COVER_OPEN}"
)
automation.register_apply_condition(
    "cover.is_closed", COVER_CONDITION_SCHEMA, f"position == {COVER_CLOSED}"
)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config: ConfigType) -> None:
    cg.add_global(cover_ns.using)
