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
    CONF_ON_OPEN,
    CONF_POSITION,
    CONF_POSITION_COMMAND_TOPIC,
    CONF_POSITION_STATE_TOPIC,
    CONF_STATE,
    CONF_STOP,
    CONF_TRIGGER_ID,
    CONF_WEB_SERVER,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_GAS,
    DEVICE_CLASS_WATER,
)
from esphome.core import CORE, CoroPriority, Lambda, coroutine_with_priority
from esphome.core.entity_helpers import (
    entity_duplicate_validator,
    queue_entity_register,
    setup_device_class,
    setup_entity,
)
from esphome.cpp_generator import LambdaExpression, MockObjClass

IS_PLATFORM_COMPONENT = True

CODEOWNERS = ["@esphome/core"]

DEVICE_CLASSES = [
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_GAS,
    DEVICE_CLASS_WATER,
]

valve_ns = cg.esphome_ns.namespace("valve")

Valve = valve_ns.class_("Valve", cg.EntityBase)
ValveCall = valve_ns.class_("ValveCall")

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

_VALVE_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA)
    .extend(
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
)


_VALVE_SCHEMA.add_extra(entity_duplicate_validator("valve"))


def valve_schema(
    class_: MockObjClass = cv.UNDEFINED,
    *,
    device_class: str = cv.UNDEFINED,
    entity_category: str = cv.UNDEFINED,
    icon: str = cv.UNDEFINED,
) -> cv.Schema:
    schema = {}

    if class_ is not cv.UNDEFINED:
        schema[cv.GenerateID()] = cv.declare_id(class_)

    for key, default, validator in [
        (CONF_DEVICE_CLASS, device_class, cv.one_of(*DEVICE_CLASSES, lower=True)),
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
        (CONF_ICON, icon, cv.icon),
    ]:
        if default is not cv.UNDEFINED:
            schema[cv.Optional(key, default=default)] = validator

    return _VALVE_SCHEMA.extend(schema)


@setup_entity("valve")
async def _setup_valve_core(var, config):
    setup_device_class(config)

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

    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(var, web_server_config)


async def register_valve(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    queue_entity_register("valve", config)
    CORE.register_platform_component("valve", var)
    await _setup_valve_core(var, config)


async def new_valve(config, *args):
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await register_valve(var, config)
    return var


VALVE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(Valve),
    }
)


@automation.register_action(
    "valve.open", OpenAction, VALVE_ACTION_SCHEMA, synchronous=True
)
async def valve_open_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "valve.close", CloseAction, VALVE_ACTION_SCHEMA, synchronous=True
)
async def valve_close_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "valve.stop", StopAction, VALVE_ACTION_SCHEMA, synchronous=True
)
async def valve_stop_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "valve.toggle", ToggleAction, VALVE_ACTION_SCHEMA, synchronous=True
)
async def valve_toggle_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


VALVE_CONTROL_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Valve),
        cv.Optional(CONF_STOP): cv.templatable(cv.boolean),
        cv.Exclusive(CONF_STATE, "pos"): cv.templatable(validate_valve_state),
        cv.Exclusive(CONF_POSITION, "pos"): cv.templatable(cv.percentage),
    }
)


@automation.register_action(
    "valve.control", ControlAction, VALVE_CONTROL_ACTION_SCHEMA, synchronous=True
)
async def valve_control_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])

    # All configured fields are folded into a single stateless lambda whose
    # constants live in flash; the action stores only a function pointer.
    # CONF_STATE and CONF_POSITION are cv.Exclusive in the schema, so at most
    # one is present and both dispatch to set_position.
    FIELDS = (
        (CONF_STOP, "set_stop", cg.bool_),
        (CONF_STATE, "set_position", cg.float_),
        (CONF_POSITION, "set_position", cg.float_),
    )

    # Normalize trigger args to `const std::remove_cvref_t<T> &` so the
    # apply lambda and any inner field lambdas (generated below via
    # `process_lambda`) share one parameter spelling that's well-formed for
    # any T (value, ref, or const-ref). Matches ControlAction::ApplyFn.
    normalized_args = [
        (cg.RawExpression(f"const std::remove_cvref_t<{cg.safe_exp(t)}> &"), n)
        for t, n in args
    ]

    fwd_args = ", ".join(name for _, name in args)
    body_lines: list[str] = []
    for conf_key, setter, type_ in FIELDS:
        if (value := config.get(conf_key)) is None:
            continue
        if isinstance(value, Lambda):
            inner = await cg.process_lambda(value, normalized_args, return_type=type_)
            body_lines.append(f"call.{setter}(({inner})({fwd_args}));")
        else:
            body_lines.append(f"call.{setter}({cg.safe_exp(value)});")

    apply_args = [
        (ValveCall.operator("ref"), "call"),
        *normalized_args,
    ]
    apply_lambda = LambdaExpression(
        ["\n".join(body_lines)],
        apply_args,
        capture="",
        return_type=cg.void,
    )
    return cg.new_Pvariable(action_id, template_arg, paren, apply_lambda)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_global(valve_ns.using)
