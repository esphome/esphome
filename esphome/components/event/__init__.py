from collections.abc import Callable
from dataclasses import dataclass, field
from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components import mqtt, web_server
import esphome.config_validation as cv
from esphome.const import (
    CONF_DATA,
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_EVENT_TYPE,
    CONF_EVENT_TYPES,
    CONF_ICON,
    CONF_ID,
    CONF_MAX_LENGTH,
    CONF_MQTT_ID,
    CONF_ON_EVENT,
    CONF_THEN,
    CONF_TYPE,
    CONF_VARIABLES,
    CONF_WEB_SERVER,
    DEVICE_CLASS_BUTTON,
    DEVICE_CLASS_DOORBELL,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_MOTION,
)
from esphome.core import (
    CORE,
    ID,
    CoroPriority,
    EsphomeError,
    Lambda,
    coroutine_with_priority,
)
from esphome.core.entity_helpers import (
    entity_duplicate_validator,
    queue_entity_register,
    setup_device_class,
    setup_entity,
)
from esphome.cpp_generator import MockObj, MockObjClass, VariableDeclarationExpression
from esphome.types import ConfigType, TemplateArgsType

CODEOWNERS = ["@nohat"]
DOMAIN = "event"
IS_PLATFORM_COMPONENT = True

DEVICE_CLASSES = [
    DEVICE_CLASS_BUTTON,
    DEVICE_CLASS_DOORBELL,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_MOTION,
]

event_ns = cg.esphome_ns.namespace("event")
Event = event_ns.class_("Event", cg.EntityBase)
EventPtr = Event.operator("ptr")

validate_device_class = cv.one_of(*DEVICE_CLASSES, lower=True, space="_")

EventVariable = event_ns.struct("EventVariable")
EventDataBlock = event_ns.struct("EventDataBlock")
EventVariableType = event_ns.enum("EventVariableType")

# Limits of event_data.h
EVENT_MAX_VARIABLES = 16
EVENT_DATA_MAX_TYPES = 127
EVENT_MAX_STRING_LENGTH = 127
# Fits the hex text of 31 bytes, the most a BTHome command carries
DEFAULT_STRING_LENGTH = 64

# Sized to the event with the most variables, for the arrays of the API messages
_request_variable_slot = cg.slot_counter("ESPHOME_EVENT_MAX_VARIABLES")

# Home Assistant shows the values next to these attributes, so the names would clash
RESERVED_VARIABLE_NAMES = (
    "assumed_state",
    "attribution",
    "device_class",
    "entity_picture",
    "event_type",
    "event_types",
    "friendly_name",
    "icon",
    "supported_features",
    "unit_of_measurement",
)


@dataclass(frozen=True)
class _VariableType:
    enum: MockObj
    arg_type: MockObj  # of the variable in on_event
    # Owns its value: of a lambda in event.trigger, and of the variable in an on_event that waits
    value_type: MockObj
    getter: str  # EventData method that reads it
    validator: Callable[[Any], Any]  # for a constant in event.trigger
    size: int  # bytes in the data block; a text adds its max_length


def _reject_bool(value: Any) -> Any:
    """A bool is an int in Python, so the number validators would take true and false."""
    if isinstance(value, bool):
        raise cv.Invalid("Expected a number, got a boolean")
    return value


_VARIABLE_TYPES: dict[str, _VariableType] = {
    "bool": _VariableType(
        EventVariableType.EVENT_VARIABLE_TYPE_BOOL,
        cg.bool_,
        cg.bool_,
        "get_bool",
        cv.boolean,
        1,
    ),
    "int": _VariableType(
        EventVariableType.EVENT_VARIABLE_TYPE_INT,
        cg.int32,
        cg.int32,
        "get_int",
        # Not -2**31: C++ reads -2147483648 as a 64-bit literal, which trigger() does not take
        cv.All(_reject_bool, cv.int_range(min=-(2**31) + 1, max=2**31 - 1)),
        4,
    ),
    "float": _VariableType(
        EventVariableType.EVENT_VARIABLE_TYPE_FLOAT,
        cg.float_,
        cg.float_,
        "get_float",
        cv.All(_reject_bool, cv.float_),
        4,
    ),
    "string": _VariableType(
        EventVariableType.EVENT_VARIABLE_TYPE_STRING,
        cg.StringRef,  # into the data block, which the next event changes
        cg.std_string,
        "get_string",
        cv.string_strict,
        1,  # the terminating zero
    ),
}


def _validate_variable_names(value: ConfigType) -> ConfigType:
    """Names become C++ variables in on_event and attribute names in Home Assistant."""
    if len(value) > EVENT_MAX_VARIABLES:
        raise cv.Invalid(f"An event has at most {EVENT_MAX_VARIABLES} variables")
    for name in value:
        if name in RESERVED_VARIABLE_NAMES:
            raise cv.Invalid(
                f"'{name}' cannot be a variable name, Home Assistant uses it for the event",
                [name],
            )
        try:
            cv.validate_id_name(name)
        except cv.Invalid as err:
            raise cv.Invalid(err.msg, [name]) from err
    return value


def _validate_max_length(value: ConfigType) -> ConfigType:
    if value[CONF_TYPE] != "string":
        if CONF_MAX_LENGTH in value:
            raise cv.Invalid("max_length is only for string", [CONF_MAX_LENGTH])
        return value
    return {CONF_MAX_LENGTH: DEFAULT_STRING_LENGTH, **value}


_VARIABLE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_TYPE): cv.one_of(*_VARIABLE_TYPES, lower=True),
            # In bytes of UTF-8
            cv.Optional(CONF_MAX_LENGTH): cv.int_range(
                min=1, max=EVENT_MAX_STRING_LENGTH
            ),
            cv.Optional(CONF_EVENT_TYPES): cv.All(
                cv.ensure_list(cv.string_strict), cv.Length(min=1)
            ),
        }
    ),
    _validate_max_length,
)

# Variables by name, in the short form `name: type` or the full form with type and event_types
validate_variables = cv.All(
    cv.Schema(
        {cv.string_strict: cv.maybe_simple_value(_VARIABLE_SCHEMA, key=CONF_TYPE)}
    ),
    _validate_variable_names,
)


def check_variables(variables: ConfigType, event_types: list[str]) -> None:
    """Check validated variables against the event types of their event."""
    if variables and len(event_types) > EVENT_DATA_MAX_TYPES:
        raise cv.Invalid(
            f"An event with variables has at most {EVENT_DATA_MAX_TYPES} event types",
            [CONF_EVENT_TYPES],
        )
    for name, variable in variables.items():
        for event_type in variable.get(CONF_EVENT_TYPES, []):
            if event_type not in event_types:
                raise cv.Invalid(
                    f"'{event_type}' is not one of the event types",
                    [CONF_VARIABLES, name, CONF_EVENT_TYPES],
                )


@dataclass
class _EventData:
    # The variables of each event that has some, by event id, for event.trigger
    variables: dict[str, ConfigType] = field(default_factory=dict)


def _get_data() -> _EventData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = _EventData()
    return CORE.data[DOMAIN]


_EVENT_SCHEMA = (
    cv.ENTITY_BASE_SCHEMA.extend(web_server.WEBSERVER_SORTING_SCHEMA)
    .extend(cv.MQTT_COMPONENT_SCHEMA)
    .extend(
        {
            cv.OnlyWith(CONF_MQTT_ID, "mqtt"): cv.declare_id(mqtt.MQTTEventComponent),
            cv.GenerateID(): cv.declare_id(Event),
            cv.Optional(
                CONF_DEVICE_CLASS, visibility=cv.Visibility.ADVANCED
            ): validate_device_class,
            cv.Optional(CONF_ON_EVENT): automation.validate_automation({}),
        }
    )
)


_EVENT_SCHEMA.add_extra(entity_duplicate_validator("event"))


def event_schema(
    class_: MockObjClass = cv.UNDEFINED,
    *,
    icon: str = cv.UNDEFINED,
    entity_category: str = cv.UNDEFINED,
    device_class: str = cv.UNDEFINED,
) -> cv.Schema:
    schema = {}

    if class_ is not cv.UNDEFINED:
        schema[cv.GenerateID()] = cv.declare_id(class_)

    for key, default, validator in [
        (CONF_ICON, icon, cv.icon),
        (CONF_ENTITY_CATEGORY, entity_category, cv.entity_category),
        (CONF_DEVICE_CLASS, device_class, validate_device_class),
    ]:
        if default is not cv.UNDEFINED:
            schema[cv.Optional(key, default=default)] = validator

    return _EVENT_SCHEMA.extend(schema)


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_EVENT, "add_on_event_callback", [(cg.StringRef, "event_type")]
    ),
)


def _add_data_block(
    var: MockObj, config: ConfigType, event_types: list[str], variables: ConfigType
) -> None:
    """The variables as a table in flash, and a data block for the values in .bss."""
    cg.add_define("USE_EVENT_DATA")
    entries = []
    offset = 0
    for name, variable in variables.items():
        _request_variable_slot(config[CONF_ID].id)
        type_ = _VARIABLE_TYPES[variable[CONF_TYPE]]
        allowed = cg.nullptr
        if (types := variable.get(CONF_EVENT_TYPES)) is not None:
            bits = [0] * ((len(event_types) + 7) // 8)
            for event_type in types:
                index = event_types.index(event_type)
                bits[index // 8] |= 1 << (index % 8)
            # The length first, so a type added at run time is not read past the end
            allowed = cg.shared_progmem_array(
                "event_variable_types", cg.uint8, cg.ArrayInitializer(len(bits), *bits)
            )
        max_length = variable.get(CONF_MAX_LENGTH, 0)
        entries.append(
            cg.ArrayInitializer(
                cg.shared_progmem_string(name), allowed, offset, type_.enum, max_length
            )
        )
        offset += type_.size + max_length
    table = cg.shared_progmem_array(
        "event_variables", EventVariable, cg.ArrayInitializer(*entries)
    )
    block = ID(
        f"{config[CONF_ID].id}__data",
        is_declaration=True,
        type=EventDataBlock.template(offset),
    )
    cg.add_global(VariableDeclarationExpression(block.type, "", block, static=True))
    cg.add(var.set_data(cg.RawExpression(f"&{block}"), table, len(variables)))


async def _build_on_event_with_data(
    var: MockObj, config: ConfigType, variables: ConfigType
) -> None:
    """on_event gets each variable by name; the callback still carries only the type."""
    data = automation.parent_ref(var).get_last_event_data()
    params = [(cg.StringRef, "event_type")]
    forward = ["event_type"]
    for index, variable in enumerate(variables.values()):
        forward.append(
            getattr(data, _VARIABLE_TYPES[variable[CONF_TYPE]].getter)(index)
        )
    for conf in config.get(CONF_ON_EVENT, []):
        # Actions that wait keep their own copy of each value
        waits = automation.has_non_synchronous_actions(conf.get(CONF_THEN, []))
        args = [*params]
        for name, variable in variables.items():
            type_ = _VARIABLE_TYPES[variable[CONF_TYPE]]
            args.append((type_.value_type if waits else type_.arg_type, name))
        await automation.build_callback_automation(
            var, "add_on_event_callback", args, conf, params=params, forward=forward
        )


@setup_entity("event")
async def setup_event_core_(
    var: MockObj,
    config: ConfigType,
    *,
    event_types: list[str],
    variables: ConfigType,
) -> None:
    if variables:
        _add_data_block(var, config, event_types, variables)
        await _build_on_event_with_data(var, config, variables)
    else:
        await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)

    if event_types:
        table = cg.shared_progmem_array("event_types", cg.const_char_ptr, event_types)
        cg.add(var.set_event_types_static(table, len(event_types)))

    setup_device_class(config)

    if mqtt_id := config.get(CONF_MQTT_ID):
        mqtt_ = cg.new_Pvariable(mqtt_id, var)
        await mqtt.register_mqtt_component(mqtt_, config)

    if web_server_config := config.get(CONF_WEB_SERVER):
        await web_server.add_entity_config(var, web_server_config)


async def register_event(
    var: MockObj,
    config: ConfigType,
    *,
    event_types: list[str],
    variables: ConfigType | None = None,
) -> None:
    """Register an event; ``variables`` takes the YAML form, e.g. ``{"steps": "int"}``."""
    variables = validate_variables(variables or {})
    check_variables(variables, event_types)
    if variables:
        # Before the first await, so an event.trigger waiting for the event finds them
        _get_data().variables[config[CONF_ID].id] = variables
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    queue_entity_register("event", config)
    CORE.register_platform_component("event", var)
    await setup_event_core_(var, config, event_types=event_types, variables=variables)


async def new_event(
    config: ConfigType,
    *,
    event_types: list[str],
    variables: ConfigType | None = None,
) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID])
    await register_event(var, config, event_types=event_types, variables=variables)
    return var


def _validate_data_value(value: Any) -> Any:
    """One bool, number or text; the type is checked against the variable at codegen."""
    if value is None or isinstance(value, (list, dict)):
        raise cv.Invalid("A variable takes a single value")
    return value


TRIGGER_EVENT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(Event),
        cv.Required(CONF_EVENT_TYPE): cv.templatable(cv.string_strict),
        cv.Optional(CONF_DATA): cv.Schema(
            {cv.string_strict: cv.templatable(_validate_data_value)}
        ),
    }
)


def _literal(config: ConfigType, value: Any) -> str:
    """A constant is a plain literal, not a std::string: trigger() is done with it when it returns."""
    return str(cg.safe_exp(value))


_trigger = automation.apply_action_builder(
    "event.trigger",
    TRIGGER_EVENT_SCHEMA,
    automation.ApplyField(CONF_EVENT_TYPE, "trigger", cg.std_string, _literal),
)


@automation.register_action(
    "event.trigger", automation.ApplyAction, TRIGGER_EVENT_SCHEMA, synchronous=True
)
async def event_trigger_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> MockObj:
    if not (data := config.get(CONF_DATA)):
        return await _trigger(config, action_id, template_arg, args)
    event_id = config[CONF_ID]
    await cg.get_variable(event_id)  # its variables are known once it exists
    if (variables := _get_data().variables.get(event_id.id)) is None:
        raise EsphomeError(f"event.trigger: event '{event_id}' has no variables")
    event_type = config[CONF_EVENT_TYPE]
    names = list(variables)
    values: dict[str, Any] = {}
    members = [(CONF_EVENT_TYPE, cg.std_string, _literal)]
    parts = []
    for key, value in data.items():
        name = str(key)
        if (variable := variables.get(name)) is None:
            raise EsphomeError(
                f"event.trigger: event '{event_id}' has no variable '{name}'"
            )
        types = variable.get(CONF_EVENT_TYPES)
        # A lambda's event type is checked on the device
        if (
            isinstance(event_type, str)
            and types is not None
            and event_type not in types
        ):
            raise EsphomeError(
                f"event.trigger: event type '{event_type}' does not carry '{name}'"
            )
        type_ = _VARIABLE_TYPES[variable[CONF_TYPE]]
        if not isinstance(value, Lambda):
            try:
                value = type_.validator(value)
            except cv.Invalid as err:
                raise EsphomeError(f"event.trigger: {name}: {err.msg}") from err
            max_length = variable.get(CONF_MAX_LENGTH)
            if max_length is not None and len(value.encode("utf-8")) > max_length:
                raise EsphomeError(
                    f"event.trigger: {name} takes at most {max_length} bytes"
                )
        values[name] = value
        # trigger() stores the value before a lambda's temporary std::string ends
        members.append(((CONF_DATA, name), type_.value_type, _literal))
        parts.append("{{" + str(names.index(name)) + ", {}}}")
    # The builder checks each key against a schema; the data keys are only known here
    schema = cv.Schema(
        {
            cv.Required(CONF_ID): cv.valid,
            cv.Required(CONF_EVENT_TYPE): cv.valid,
            cv.Optional(CONF_DATA): {cv.Optional(name): cv.valid for name in values},
        }
    )
    builder = automation.apply_action_builder(
        "event.trigger",
        schema,
        automation.ApplyCall(
            "trigger({}, {{" + ", ".join(parts) + "}})", tuple(members)
        ),
    )
    return await builder({**config, CONF_DATA: values}, action_id, template_arg, args)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config: ConfigType) -> None:
    cg.add_global(event_ns.using)
