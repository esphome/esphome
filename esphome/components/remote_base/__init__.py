from collections.abc import Callable
from pathlib import Path
from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_BATTERY_LEVEL,
    CONF_BUTTON,
    CONF_CARRIER_FREQUENCY,
    CONF_CHANNEL,
    CONF_CHANNELS,
    CONF_CHECK,
    CONF_CODE,
    CONF_COMMAND,
    CONF_COMMAND_REPEATS,
    CONF_DATA,
    CONF_DELTA,
    CONF_DEVICE,
    CONF_FAMILY,
    CONF_FAN_MODE,
    CONF_FORCE_UPDATE,
    CONF_GROUP,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_INDEX,
    CONF_INVERTED,
    CONF_KEY,
    CONF_LEVEL,
    CONF_MAGNITUDE,
    CONF_MODE,
    CONF_NBITS,
    CONF_ONE,
    CONF_PROTOCOL,
    CONF_PULSE_LENGTH,
    CONF_RC_CODE_1,
    CONF_RC_CODE_2,
    CONF_REPEAT,
    CONF_SECOND,
    CONF_SOURCE,
    CONF_STATE,
    CONF_SYNC,
    CONF_TEMPERATURE,
    CONF_TIMES,
    CONF_TRIGGER_ID,
    CONF_TYPE_ID,
    CONF_WAIT_TIME,
    CONF_WAND_ID,
    CONF_ZERO,
)
from esphome.core import ID, coroutine
from esphome.cpp_generator import MockObj
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType, SafeExpType, TemplateArgsType
from esphome.util import Registry, SimpleRegistry

AUTO_LOAD = ["binary_sensor"]
DOMAIN = "remote_base"


CONF_ADDRESS_2 = "address_2"
CONF_RECEIVER_ID = "receiver_id"
CONF_TRANSMITTER_ID = "transmitter_id"
CONF_FIRST = "first"
CONF_SUFFIX = "suffix"
CONF_SUFFIX_BITS = "suffix_bits"
CONF_CONTROL = "control"
CONF_CONTROL_IN_KEY = "control_in_key"
CONF_ROLLING_CODE = "rolling_code"
CONF_EXTENSION = "extension"
CONF_REMOTE_ID = "remote_id"

ns = remote_base_ns = cg.esphome_ns.namespace("remote_base")
RemoteProtocol = ns.class_("RemoteProtocol")
RemoteReceiverListener = ns.class_("RemoteReceiverListener")
RemoteReceiverBinarySensorBase = ns.class_(
    "RemoteReceiverBinarySensorBase", binary_sensor.BinarySensor, cg.Component
)
RemoteReceiverTrigger = ns.class_(
    "RemoteReceiverTrigger", automation.Trigger, RemoteReceiverListener
)
RemoteReceiverDumperBase = ns.class_("RemoteReceiverDumperBase")
RemoteTransmittable = ns.class_("RemoteTransmittable")
RemoteTransmitterActionBase = ns.class_(
    "RemoteTransmitterActionBase", RemoteTransmittable, automation.Action
)
RemoteReceiverBase = ns.class_("RemoteReceiverBase")
RemoteTransmitterBase = ns.class_("RemoteTransmitterBase")


def templatize(value):
    if isinstance(value, cv.Schema):
        value = value.schema
    ret = {}
    for key, val in value.items():
        ret[key] = cv.templatable(val)
    return cv.Schema(ret)


REMOTE_LISTENER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_RECEIVER_ID): cv.use_id(RemoteReceiverBase),
    }
)


REMOTE_TRANSMITTABLE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TRANSMITTER_ID): cv.use_id(RemoteTransmitterBase),
    }
)


# Listener and dumper lists are StaticVectors sized from these counts, so every registration
# must go through add_listener / add_dumper. Every receiver's list gets the same capacity, so
# the slots are keyed by receiver and the define is the largest count any one receiver needs.
LISTENER_COUNT_DEFINE = "REMOTE_BASE_LISTENER_COUNT"
DUMPER_COUNT_DEFINE = "REMOTE_BASE_DUMPER_COUNT"


_request_listener_slot = cg.slot_counter(LISTENER_COUNT_DEFINE)
_request_dumper_slot = cg.slot_counter(DUMPER_COUNT_DEFINE)


def add_listener(receiver: MockObj, listener: MockObj) -> None:
    _request_listener_slot(str(receiver))
    cg.add(receiver.register_listener(listener))


def add_dumper(receiver: MockObj, dumper: MockObj) -> None:
    _request_dumper_slot(str(receiver))
    cg.add(receiver.register_dumper(dumper))


async def register_listener(var: MockObj, config: ConfigType) -> None:
    receiver = await cg.get_variable(config[CONF_RECEIVER_ID])
    add_listener(receiver, var)


async def attach_receiver(
    var: MockObj, config: ConfigType, key: str = CONF_RECEIVER_ID
) -> None:
    """Link the configured receiver to an entity and register the entity as its listener.

    The C++ set_receiver() no longer registers the listener; the slot for it is counted here.
    """
    receiver = await cg.get_variable(config[key])
    cg.add(var.set_receiver(receiver))
    add_listener(receiver, var)


async def register_transmittable(var, config, key: str = CONF_TRANSMITTER_ID):
    transmitter_ = await cg.get_variable(config[key])
    cg.add(var.set_transmitter(transmitter_))


# Registry names that share a protocol source file
def _protocol_stem(name: str) -> str:
    if name.startswith("rc_switch"):
        return "rc_switch"
    if name == "canalsatld":
        return "canalsat"
    return name


def protocol_define(name: str) -> str:
    return f"USE_REMOTE_PROTOCOL_{_protocol_stem(name).upper()}"


_PROTOCOL_STEMS = sorted(
    path.name.removesuffix("_protocol.cpp")
    for path in Path(__file__).parent.glob("*_protocol.cpp")
)


def request_protocol(name: str) -> None:
    """Keep a protocol's source file in the build; components using it from C++ must call this."""
    if _protocol_stem(name) not in _PROTOCOL_STEMS:
        raise ValueError(
            f"Unknown remote protocol {name!r}; expected one of {', '.join(_PROTOCOL_STEMS)}"
        )
    cg.add_define(protocol_define(name))


def _request_protocol_if_in_tree(name: str) -> None:
    """Registry names from external components have no source file here and need no define."""
    if _protocol_stem(name) in _PROTOCOL_STEMS:
        request_protocol(name)


# Only the protocol sources a configuration uses are compiled
FILTER_SOURCE_FILES = filter_source_files_from_defines(
    {f"{stem}_protocol.cpp": protocol_define(stem) for stem in _PROTOCOL_STEMS}
)


def register_binary_sensor(
    name: str, type: MockObj, schema: cv.Schema | dict
) -> Callable[[Callable[[MockObj, ConfigType], Any]], Callable]:
    registerer = BINARY_SENSOR_REGISTRY.register(name, type, schema)

    def decorator(func: Callable[[MockObj, ConfigType], Any]) -> Callable:
        async def new_func(var: MockObj, config: ConfigType) -> None:
            _request_protocol_if_in_tree(name)
            await coroutine(func)(var, config)

        return registerer(new_func)

    return decorator


def register_trigger(name, type, data_type):
    validator = automation.validate_automation(
        {
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(type),
        }
    )
    registerer = TRIGGER_REGISTRY.register(f"on_{name}", validator)

    def decorator(func):
        async def new_func(config):
            _request_protocol_if_in_tree(name)
            var = cg.new_Pvariable(config[CONF_TRIGGER_ID])
            await coroutine(func)(var, config)
            await automation.build_automation(var, [(data_type, "x")], config)
            return var

        return registerer(new_func)

    return decorator


def register_dumper(name, type, schema=None):
    if schema is None:
        schema = {}
    registerer = DUMPER_REGISTRY.register(name, type, schema)

    def decorator(func):
        async def new_func(config, dumper_id):
            _request_protocol_if_in_tree(name)
            var = cg.new_Pvariable(dumper_id)
            await coroutine(func)(var, config)
            return var

        return registerer(new_func)

    return decorator


def validate_repeat(value):
    if isinstance(value, dict):
        return cv.Schema(
            {
                cv.Required(CONF_TIMES): cv.templatable(cv.positive_int),
                cv.Optional(CONF_WAIT_TIME, default="25ms"): cv.templatable(
                    cv.positive_time_period_microseconds
                ),
            }
        )(value)
    return validate_repeat({CONF_TIMES: value})


BASE_REMOTE_TRANSMITTER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_REPEAT): validate_repeat,
    }
).extend(REMOTE_TRANSMITTABLE_SCHEMA)


def _transmit_action(
    name: str,
    schema: cv.Schema,
    validate: Callable[[ConfigType], ConfigType] | None,
) -> tuple[str, cv.Schema, Any]:
    """The action name, its schema and the schema with ``validate`` applied."""
    schema = templatize(schema).extend(BASE_REMOTE_TRANSMITTER_SCHEMA)
    validator = schema if validate is None else cv.All(schema, validate)
    return f"remote_transmitter.transmit_{name}", schema, validator


def register_action(
    name, type_, schema, *, validate: Callable[[ConfigType], ConfigType] | None = None
):
    action, _, validator = _transmit_action(name, schema, validate)
    registerer = automation.register_action(
        action,
        type_,
        validator,
        synchronous=True,
    )

    def decorator(func):
        async def new_func(config, action_id, template_arg, args):
            _request_protocol_if_in_tree(name)
            var = cg.new_Pvariable(action_id, template_arg)
            await register_transmittable(var, config)
            if CONF_REPEAT in config:
                conf = config[CONF_REPEAT]
                template_ = await cg.templatable(conf[CONF_TIMES], args, cg.uint32)
                cg.add(var.set_send_times(template_))
                template_ = await cg.templatable(conf[CONF_WAIT_TIME], args, cg.uint32)
                cg.add(var.set_send_wait(template_))
            await coroutine(func)(var, config, args)
            return var

        return registerer(new_func)

    return decorator


ProtocolField = (
    tuple[str, str, SafeExpType] | automation.ApplyField | automation.ApplyCall
)


def _protocol_field(
    field: ProtocolField,
) -> automation.ApplyField | automation.ApplyCall:
    if not isinstance(field, tuple):
        return field
    key, member, type_ = field
    return automation.ApplyField(key, f"data.{member} = {{}}", type_)


_BYTES = cg.std_vector.template(cg.uint8)


def _code_table(config: ConfigType, value: list[int]) -> str:
    return f"{automation.progmem_bytes('remote_base_code', value)}, {len(value)}"


def _bytes_field(conf_key: str, target: str) -> automation.ApplyField:
    """A constant's flash table and length, or a lambda's vector; the callee takes both."""
    return automation.ApplyField(conf_key, target, _BYTES, const_fn=_code_table)


def register_protocol_action(
    name: str,
    protocol: str,
    schema: cv.Schema,
    *fields: ProtocolField,
    validate: Callable[[ConfigType], ConfigType] | None = None,
    call: str = "protocol_call",
) -> None:
    """Register ``transmit_<name>`` with no C++ action class.

    One generated function fills ``<protocol>Data`` from ``(conf_key, member, type_)`` fields
    and sends it; the action is a core ``ApplyAction``. ``ApplyField`` and ``ApplyCall`` pass through
    and target the call object, whose protocol data is ``data``.
    ``call="encode_call"`` targets a plain transmit call whose ``encode()`` runs one of the
    protocol's encoders, for payloads it encodes straight from a pointer and length.
    """
    action, schema, validator = _transmit_action(name, schema, validate)
    builder = automation.apply_action_builder(
        action,
        schema,
        *(_protocol_field(field) for field in fields),
        automation.ApplyField((CONF_REPEAT, CONF_TIMES), "set_send_times", cg.uint32),
        automation.ApplyField(
            (CONF_REPEAT, CONF_WAIT_TIME), "set_send_wait", cg.uint32
        ),
        call=f"{call}<{ns.class_(f'{protocol}Protocol')}>",
        id_key=CONF_TRANSMITTER_ID,
    )

    async def protocol_builder(
        config: ConfigType,
        action_id: ID,
        template_arg: cg.TemplateArguments,
        args: TemplateArgsType,
    ) -> MockObj:
        _request_protocol_if_in_tree(name)
        return await builder(config, action_id, template_arg, args)

    automation.register_action(
        action, automation.ApplyAction, validator, synchronous=True
    )(protocol_builder)


def declare_protocol(name):
    data = ns.struct(f"{name}Data")
    binary_sensor_ = ns.class_(f"{name}BinarySensor", RemoteReceiverBinarySensorBase)
    trigger = ns.class_(f"{name}Trigger", RemoteReceiverTrigger)
    action = ns.class_(f"{name}Action", RemoteTransmitterActionBase)
    dumper = ns.class_(f"{name}Dumper", RemoteReceiverDumperBase)
    return data, binary_sensor_, trigger, action, dumper


BINARY_SENSOR_REGISTRY = Registry(
    binary_sensor.binary_sensor_schema().extend(
        {
            cv.GenerateID(CONF_RECEIVER_ID): cv.use_id(RemoteReceiverBase),
        }
    )
)
validate_binary_sensor = cv.validate_registry_entry(
    "remote receiver", BINARY_SENSOR_REGISTRY
)
TRIGGER_REGISTRY = SimpleRegistry()
DUMPER_REGISTRY = Registry()


def validate_dumpers(value):
    if isinstance(value, str) and value.lower() == "all":
        return validate_dumpers(list(DUMPER_REGISTRY.keys()))
    entries = cv.validate_registry("dumper", DUMPER_REGISTRY)(value)
    # a dumper listed twice would register twice; the receiver holds one secondary dumper
    return list(
        {
            next(k for k in entry if k in DUMPER_REGISTRY): entry for entry in entries
        }.values()
    )


def validate_triggers(base_schema):
    assert isinstance(base_schema, cv.Schema)

    @schema_extractor("triggers")
    def validator(config):
        added_keys = {}
        for key, (_, valid) in TRIGGER_REGISTRY.items():
            added_keys[cv.Optional(key)] = valid
        new_schema = base_schema.extend(added_keys)

        if config == SCHEMA_EXTRACT:
            return new_schema
        return new_schema(config)

    return validator


async def build_binary_sensor(full_config):
    registry_entry, config = cg.extract_registry_entry_config(
        BINARY_SENSOR_REGISTRY, full_config
    )
    type_id = full_config[CONF_TYPE_ID]
    builder = registry_entry.coroutine_fun
    var = cg.new_Pvariable(type_id)
    await cg.register_component(var, full_config)
    await register_listener(var, full_config)
    await builder(var, config)
    return var


async def build_triggers(full_config):
    triggers = []
    for key in TRIGGER_REGISTRY:
        for config in full_config.get(key, []):
            func = TRIGGER_REGISTRY[key][0]
            triggers.append(await func(config))
    return triggers


async def build_dumpers(config):
    dumpers = []
    for conf in config:
        dumper = await cg.build_registry_entry(DUMPER_REGISTRY, conf)
        dumpers.append(dumper)
    return dumpers


# Beo4
Beo4Data, Beo4BinarySensor, Beo4Trigger, _, Beo4Dumper = declare_protocol("Beo4")
BEO4_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SOURCE): cv.hex_uint8_t,
        cv.Required(CONF_COMMAND): cv.hex_uint8_t,
        cv.Optional(CONF_COMMAND_REPEATS, default=1): cv.uint8_t,
    }
)


@register_binary_sensor("beo4", Beo4BinarySensor, BEO4_SCHEMA)
def beo4_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                Beo4Data,
                ("source", config[CONF_SOURCE]),
                ("command", config[CONF_COMMAND]),
                ("repeats", config[CONF_COMMAND_REPEATS]),
            )
        )
    )


@register_trigger("beo4", Beo4Trigger, Beo4Data)
def beo4_trigger(var, config):
    pass


@register_dumper("beo4", Beo4Dumper)
def beo4_dumper(var, config):
    pass


register_protocol_action(
    "beo4",
    "Beo4",
    BEO4_SCHEMA,
    (CONF_SOURCE, "source", cg.uint8),
    (CONF_COMMAND, "command", cg.uint8),
    (CONF_COMMAND_REPEATS, "repeats", cg.uint8),
)


# Brennenstuhl
(
    BrennenstuhlData,
    BrennenstuhlBinarySensor,
    BrennenstuhlTrigger,
    _,
    BrennenstuhlDumper,
) = declare_protocol("Brennenstuhl")

BRENNENSTUHL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CODE): cv.hex_uint32_t,
    }
)


@register_binary_sensor("brennenstuhl", BrennenstuhlBinarySensor, BRENNENSTUHL_SCHEMA)
def brennenstuhl_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                BrennenstuhlData,
                ("code", config[CONF_CODE]),
            )
        )
    )


@register_trigger("brennenstuhl", BrennenstuhlTrigger, BrennenstuhlData)
def brennenstuhl_trigger(var, config):
    pass


@register_dumper("brennenstuhl", BrennenstuhlDumper)
def brennenstuhl_dumper(var, config):
    pass


register_protocol_action(
    "brennenstuhl",
    "Brennenstuhl",
    BRENNENSTUHL_SCHEMA,
    (CONF_CODE, "code", cg.uint32),
)


# ByronSX
(
    ByronSXData,
    ByronSXBinarySensor,
    ByronSXTrigger,
    _,
    ByronSXDumper,
) = declare_protocol("ByronSX")
BYRONSX_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.All(cv.hex_int, cv.Range(min=0, max=0xFF)),
        cv.Optional(CONF_COMMAND, default=0x10): cv.All(
            cv.hex_int, cv.one_of(1, 2, 3, 5, 6, 9, 0xD, 0xE, 0x10, int=True)
        ),
    }
)


@register_binary_sensor("byronsx", ByronSXBinarySensor, BYRONSX_SCHEMA)
def byronsx_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                ByronSXData,
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("byronsx", ByronSXTrigger, ByronSXData)
def byronsx_trigger(var, config):
    pass


@register_dumper("byronsx", ByronSXDumper)
def byronsx_dumper(var, config):
    pass


register_protocol_action(
    "byronsx",
    "ByronSX",
    BYRONSX_SCHEMA,
    (CONF_ADDRESS, "address", cg.uint8),
    (CONF_COMMAND, "command", cg.uint8),
)


# CanalSat
(
    CanalSatData,
    CanalSatBinarySensor,
    CanalSatTrigger,
    _,
    CanalSatDumper,
) = declare_protocol("CanalSat")
CANALSAT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DEVICE): cv.hex_uint8_t,
        cv.Optional(CONF_ADDRESS, default=0): cv.hex_uint8_t,
        cv.Required(CONF_COMMAND): cv.hex_uint8_t,
    }
)


@register_binary_sensor("canalsat", CanalSatBinarySensor, CANALSAT_SCHEMA)
def canalsat_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                CanalSatData,
                ("device", config[CONF_DEVICE]),
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("canalsat", CanalSatTrigger, CanalSatData)
def canalsat_trigger(var, config):
    pass


@register_dumper("canalsat", CanalSatDumper)
def canalsat_dumper(var, config):
    pass


register_protocol_action(
    "canalsat",
    "CanalSat",
    CANALSAT_SCHEMA,
    (CONF_DEVICE, "device", cg.uint8),
    (CONF_ADDRESS, "address", cg.uint8),
    (CONF_COMMAND, "command", cg.uint8),
)


(
    CanalSatLDData,
    CanalSatLDBinarySensor,
    CanalSatLDTrigger,
    _,
    CanalSatLDDumper,
) = declare_protocol("CanalSatLD")
CANALSATLD_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DEVICE): cv.hex_uint8_t,
        cv.Optional(CONF_ADDRESS, default=0): cv.hex_uint8_t,
        cv.Required(CONF_COMMAND): cv.hex_uint8_t,
    }
)


@register_binary_sensor("canalsatld", CanalSatLDBinarySensor, CANALSATLD_SCHEMA)
def canalsatld_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                CanalSatLDData,
                ("device", config[CONF_DEVICE]),
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("canalsatld", CanalSatLDTrigger, CanalSatLDData)
def canalsatld_trigger(var, config):
    pass


@register_dumper("canalsatld", CanalSatLDDumper)
def canalsatld_dumper(var, config):
    pass


register_protocol_action(
    "canalsatld",
    "CanalSatLD",
    CANALSATLD_SCHEMA,
    (CONF_DEVICE, "device", cg.uint8),
    (CONF_ADDRESS, "address", cg.uint8),
    (CONF_COMMAND, "command", cg.uint8),
)


# Coolix
(
    CoolixData,
    CoolixBinarySensor,
    CoolixTrigger,
    _,
    CoolixDumper,
) = declare_protocol("Coolix")


COOLIX_BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_FIRST): cv.hex_int_range(0, 16777215),
        cv.Optional(CONF_SECOND, default=0): cv.hex_int_range(0, 16777215),
    }
)

COOLIX_SENSOR_SCHEMA = cv.Any(cv.hex_int_range(0, 16777215), COOLIX_BASE_SCHEMA)


@register_binary_sensor("coolix", CoolixBinarySensor, COOLIX_SENSOR_SCHEMA)
def coolix_binary_sensor(var, config):
    if isinstance(config, dict):
        cg.add(
            var.set_data(
                cg.ArrayInitializer(
                    config[CONF_FIRST],
                    config[CONF_SECOND],
                )
            )
        )
    else:
        cg.add(var.set_data(cg.ArrayInitializer(0, config)))


register_protocol_action(
    "coolix",
    "Coolix",
    COOLIX_BASE_SCHEMA,
    (CONF_FIRST, "first", cg.uint32),
    (CONF_SECOND, "second", cg.uint32),
)


@register_trigger("coolix", CoolixTrigger, CoolixData)
def coolix_trigger(var, config):
    pass


@register_dumper("coolix", CoolixDumper)
def coolix_dumper(var, config):
    pass


# Dish
DishData, DishBinarySensor, DishTrigger, _, DishDumper = declare_protocol("Dish")
DISH_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ADDRESS, default=1): cv.int_range(min=1, max=16),
        cv.Required(CONF_COMMAND): cv.int_range(min=0, max=63),
    }
)


@register_binary_sensor("dish", DishBinarySensor, DISH_SCHEMA)
def dish_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                DishData,
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("dish", DishTrigger, DishData)
def dish_trigger(var, config):
    pass


@register_dumper("dish", DishDumper)
def dish_dumper(var, config):
    pass


register_protocol_action(
    "dish",
    "Dish",
    DISH_SCHEMA,
    (CONF_ADDRESS, "address", cg.uint8),
    (CONF_COMMAND, "command", cg.uint8),
)


# Dooya
DooyaData, DooyaBinarySensor, DooyaTrigger, _, DooyaDumper = declare_protocol("Dooya")
DOOYA_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.hex_int_range(0, 16777215),
        cv.Required(CONF_CHANNEL): cv.hex_int_range(0, 255),
        cv.Required(CONF_BUTTON): cv.hex_int_range(0, 15),
        cv.Required(CONF_CHECK): cv.hex_int_range(0, 15),
    }
)


@register_binary_sensor("dooya", DooyaBinarySensor, DOOYA_SCHEMA)
def dooya_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                DooyaData,
                ("id", config[CONF_ID]),
                ("channel", config[CONF_CHANNEL]),
                ("button", config[CONF_BUTTON]),
                ("check", config[CONF_CHECK]),
            )
        )
    )


@register_trigger("dooya", DooyaTrigger, DooyaData)
def dooya_trigger(var, config):
    pass


@register_dumper("dooya", DooyaDumper)
def dooya_dumper(var, config):
    pass


register_protocol_action(
    "dooya",
    "Dooya",
    DOOYA_SCHEMA,
    (CONF_ID, "id", cg.uint32),
    (CONF_CHANNEL, "channel", cg.uint8),
    (CONF_BUTTON, "button", cg.uint8),
    (CONF_CHECK, "check", cg.uint8),
)


# Dyson
DysonData, DysonBinarySensor, DysonTrigger, _, DysonDumper = declare_protocol("Dyson")
DYSON_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CODE): cv.hex_uint16_t,
        cv.Optional(CONF_INDEX, default=0xFF): cv.hex_uint8_t,
    }
)


@register_binary_sensor("dyson", DysonBinarySensor, DYSON_SCHEMA)
def dyson_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                DysonData,
                ("code", config[CONF_CODE]),
                ("index", config[CONF_INDEX]),
            )
        )
    )


@register_trigger("dyson", DysonTrigger, DysonData)
def dyson_trigger(var, config):
    pass


@register_dumper("dyson", DysonDumper)
def dyson_dumper(var, config):
    pass


register_protocol_action(
    "dyson",
    "Dyson",
    DYSON_SCHEMA,
    (CONF_CODE, "code", cg.uint16),
    (CONF_INDEX, "index", cg.uint8),
)


# JVC
JVCData, JVCBinarySensor, JVCTrigger, _, JVCDumper = declare_protocol("JVC")
JVC_SCHEMA = cv.Schema({cv.Required(CONF_DATA): cv.hex_uint32_t})


@register_binary_sensor("jvc", JVCBinarySensor, JVC_SCHEMA)
def jvc_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                JVCData,
                ("data", config[CONF_DATA]),
            )
        )
    )


@register_trigger("jvc", JVCTrigger, JVCData)
def jvc_trigger(var, config):
    pass


@register_dumper("jvc", JVCDumper)
def jvc_dumper(var, config):
    pass


register_protocol_action(
    "jvc",
    "JVC",
    JVC_SCHEMA,
    (CONF_DATA, "data", cg.uint32),
)


# LG
LGData, LGBinarySensor, LGTrigger, _, LGDumper = declare_protocol("LG")
LG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DATA): cv.hex_uint32_t,
        cv.Optional(CONF_NBITS, default=28): cv.one_of(28, 32, int=True),
    }
)


@register_binary_sensor("lg", LGBinarySensor, LG_SCHEMA)
def lg_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                LGData,
                ("data", config[CONF_DATA]),
                ("nbits", config[CONF_NBITS]),
            )
        )
    )


@register_trigger("lg", LGTrigger, LGData)
def lg_trigger(var, config):
    pass


@register_dumper("lg", LGDumper)
def lg_dumper(var, config):
    pass


register_protocol_action(
    "lg",
    "LG",
    LG_SCHEMA,
    (CONF_DATA, "data", cg.uint32),
    (CONF_NBITS, "nbits", cg.uint8),
)


# MagiQuest
(
    MagiQuestData,
    MagiQuestBinarySensor,
    MagiQuestTrigger,
    _,
    MagiQuestDumper,
) = declare_protocol("MagiQuest")

MAGIQUEST_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_WAND_ID): cv.hex_uint32_t,
        cv.Optional(CONF_MAGNITUDE, default=0xFFFF): cv.hex_uint16_t,
    }
)

# A wand_id of 0 matches any wand. Magnitude is still accepted but not matched on.
MAGIQUEST_BINARY_SENSOR_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WAND_ID, default=0): cv.hex_uint32_t,
        cv.Optional(CONF_MAGNITUDE): cv.hex_uint16_t,
    }
)


@register_binary_sensor(
    "magiquest", MagiQuestBinarySensor, MAGIQUEST_BINARY_SENSOR_SCHEMA
)
def magiquest_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(MagiQuestData, ("wand_id", config[CONF_WAND_ID]))
        )
    )


@register_trigger("magiquest", MagiQuestTrigger, MagiQuestData)
def magiquest_trigger(var, config):
    pass


@register_dumper("magiquest", MagiQuestDumper)
def magiquest_dumper(var, config):
    pass


register_protocol_action(
    "magiquest",
    "MagiQuest",
    MAGIQUEST_SCHEMA,
    (CONF_WAND_ID, "wand_id", cg.uint32),
    (CONF_MAGNITUDE, "magnitude", cg.uint16),
)


# Microchip HCS301 KeeLoq OOK
(
    KeeloqData,
    KeeloqBinarySensor,
    KeeloqTrigger,
    _,
    KeeloqDumper,
) = declare_protocol("Keeloq")
KEELOQ_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.All(cv.hex_int, cv.Range(min=0, max=0xFFFFFFF)),
        cv.Required(CONF_CODE): cv.All(cv.hex_int, cv.Range(min=0, max=0xFFFFFFFF)),
        cv.Optional(CONF_COMMAND, default=0x10): cv.All(
            cv.hex_int,
            cv.Range(min=0, max=0x10),
        ),
        cv.Optional(CONF_LEVEL, default=False): cv.boolean,
    }
)


def _keeloq_group_alone(config: ConfigType) -> ConfigType:
    # The group bits take the places of level and the suffix.
    if CONF_GROUP not in config:
        return config
    if CONF_SUFFIX in config or CONF_SUFFIX_BITS in config:
        raise cv.Invalid(
            "group cannot be combined with suffix or suffix_bits", [CONF_GROUP]
        )
    if config[CONF_LEVEL] is not False:
        raise cv.Invalid(
            "group sets the bit that level sends; remove level", [CONF_LEVEL]
        )
    return config


def _keeloq_suffix_fits(config: ConfigType) -> ConfigType:
    # A lambda is only known on the device. A plain value has to fit the width.
    suffix = config.get(CONF_SUFFIX, 0)
    bits = config.get(CONF_SUFFIX_BITS, 0)
    if isinstance(suffix, int) and isinstance(bits, int) and suffix >> bits:
        raise cv.Invalid(
            f"suffix 0x{suffix:X} does not fit in {bits} suffix_bits", [CONF_SUFFIX]
        )
    return config


KEELOQ_ACTION_SCHEMA = KEELOQ_SCHEMA.extend(
    {
        # No defaults: an absent key leaves the data at 0 and emits nothing.
        cv.Optional(CONF_SUFFIX): cv.All(cv.hex_int, cv.Range(min=0, max=0xFFFF)),
        cv.Optional(CONF_SUFFIX_BITS): cv.int_range(min=0, max=16),
        cv.Optional(CONF_GROUP): cv.hex_uint8_t,
    }
)


@register_binary_sensor("keeloq", KeeloqBinarySensor, KEELOQ_SCHEMA)
def Keeloq_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                KeeloqData,
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("keeloq", KeeloqTrigger, KeeloqData)
def keeloq_trigger(var, config):
    pass


@register_dumper("keeloq", KeeloqDumper)
def keeloq_dumper(var, config):
    pass


register_protocol_action(
    "keeloq",
    "Keeloq",
    KEELOQ_ACTION_SCHEMA,
    (CONF_ADDRESS, "address", cg.uint32),
    (CONF_CODE, "encrypted", cg.uint32),
    (CONF_COMMAND, "command", cg.uint8),
    (CONF_LEVEL, "vlow", cg.bool_),
    (CONF_SUFFIX, "suffix", cg.uint16),
    (CONF_SUFFIX_BITS, "suffix_bits", cg.uint8),
    automation.ApplyField(CONF_GROUP, "data.set_group", cg.uint8),
    validate=cv.All(_keeloq_group_alone, _keeloq_suffix_fits),
)


# NEC
NECData, NECBinarySensor, NECTrigger, _, NECDumper = declare_protocol("NEC")
NEC_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
        cv.Required(CONF_COMMAND): cv.hex_uint16_t,
        # Non-RMT transmitters hold an interrupt lock for the whole send, ~108 ms per extra repeat
        cv.Optional(CONF_COMMAND_REPEATS, default=1): cv.int_range(min=1, max=10),
    }
)


@register_binary_sensor("nec", NECBinarySensor, NEC_SCHEMA)
def nec_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                NECData,
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
                ("command_repeats", config[CONF_COMMAND_REPEATS]),
            )
        )
    )


@register_trigger("nec", NECTrigger, NECData)
def nec_trigger(var, config):
    pass


@register_dumper("nec", NECDumper)
def nec_dumper(var, config):
    pass


register_protocol_action(
    "nec",
    "NEC",
    NEC_SCHEMA,
    (CONF_ADDRESS, "address", cg.uint16),
    (CONF_COMMAND, "command", cg.uint16),
    (CONF_COMMAND_REPEATS, "command_repeats", cg.uint16),
)


# Onkyo RI
OnkyoRIData, OnkyoRIBinarySensor, OnkyoRITrigger, _, OnkyoRIDumper = declare_protocol(
    "OnkyoRI"
)
ONKYORI_SCHEMA = cv.Schema({cv.Required(CONF_DATA): cv.hex_int_range(0, 0xFFF)})


@register_binary_sensor("onkyori", OnkyoRIBinarySensor, ONKYORI_SCHEMA)
def onkyori_binary_sensor(var: MockObj, config: ConfigType) -> None:
    cg.add(
        var.set_data(
            cg.StructInitializer(
                OnkyoRIData,
                ("data", config[CONF_DATA]),
            )
        )
    )


@register_trigger("onkyori", OnkyoRITrigger, OnkyoRIData)
def onkyori_trigger(var: MockObj, config: ConfigType) -> None:
    """The trigger takes no options beyond the automation."""


@register_dumper("onkyori", OnkyoRIDumper)
def onkyori_dumper(var: MockObj, config: ConfigType) -> None:
    """The dumper takes no options."""


register_protocol_action(
    "onkyori",
    "OnkyoRI",
    ONKYORI_SCHEMA,
    (CONF_DATA, "data", cg.uint16),
)


# Pioneer
(
    PioneerData,
    PioneerBinarySensor,
    PioneerTrigger,
    _,
    PioneerDumper,
) = declare_protocol("Pioneer")
PIONEER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_RC_CODE_1): cv.hex_uint16_t,
        cv.Optional(CONF_RC_CODE_2, default=0): cv.hex_uint16_t,
    }
)


@register_binary_sensor("pioneer", PioneerBinarySensor, PIONEER_SCHEMA)
def pioneer_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                PioneerData,
                ("rc_code_1", config[CONF_RC_CODE_1]),
                ("rc_code_2", config[CONF_RC_CODE_2]),
            )
        )
    )


@register_trigger("pioneer", PioneerTrigger, PioneerData)
def pioneer_trigger(var, config):
    pass


@register_dumper("pioneer", PioneerDumper)
def pioneer_dumper(var, config):
    pass


register_protocol_action(
    "pioneer",
    "Pioneer",
    PIONEER_SCHEMA,
    (CONF_RC_CODE_1, "rc_code_1", cg.uint16),
    (CONF_RC_CODE_2, "rc_code_2", cg.uint16),
)


# PioneerWyt
PioneerWytData = ns.class_("PioneerWytData")
PIONEER_WYT_SCHEMA = cv.Schema(
    {
        # 13 bytes get the checksum appended; a 14th byte is replaced by the recomputed checksum
        cv.Required(CONF_CODE): cv.All([cv.hex_uint8_t], cv.Length(min=13, max=14)),
    }
)

register_protocol_action(
    "pioneer_wyt",
    "PioneerWyt",
    PIONEER_WYT_SCHEMA,
    _bytes_field(CONF_CODE, f"data = {PioneerWytData}({{}})"),
    automation.ApplyCall("data.finalize()"),
)


# Pronto
(
    ProntoData,
    ProntoBinarySensor,
    ProntoTrigger,
    _,
    ProntoDumper,
) = declare_protocol("Pronto")
PRONTO_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DATA): cv.string,
        cv.Optional(CONF_DELTA, default=-1): cv.int_,
    }
)


@register_binary_sensor("pronto", ProntoBinarySensor, PRONTO_SCHEMA)
def pronto_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                ProntoData,
                ("data", config[CONF_DATA]),
                ("delta", config[CONF_DELTA]),
            )
        )
    )


@register_trigger("pronto", ProntoTrigger, ProntoData)
def pronto_trigger(var, config):
    pass


@register_dumper("pronto", ProntoDumper)
def pronto_dumper(var, config):
    pass


# delta only matters when matching a received code
register_protocol_action(
    "pronto", "Pronto", PRONTO_SCHEMA, (CONF_DATA, "data", cg.std_string)
)


# Gobox
(
    GoboxData,
    GoboxBinarySensor,
    GoboxTrigger,
    _,
    GoboxDumper,
) = declare_protocol("Gobox")
GOBOX_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CODE): cv.int_,
    }
)


@register_binary_sensor("gobox", GoboxBinarySensor, GOBOX_SCHEMA)
def gobox_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                GoboxData,
                ("code", config[CONF_CODE]),
            )
        )
    )


@register_trigger("gobox", GoboxTrigger, GoboxData)
def gobox_trigger(var, config):
    pass


@register_dumper("gobox", GoboxDumper)
def gobox_dumper(var, config):
    pass


register_protocol_action(
    "gobox",
    "Gobox",
    GOBOX_SCHEMA,
    (CONF_CODE, "code", cg.uint64),
)


# Roomba
(
    RoombaData,
    RoombaBinarySensor,
    RoombaTrigger,
    _,
    RoombaDumper,
) = declare_protocol("Roomba")
ROOMBA_SCHEMA = cv.Schema({cv.Required(CONF_DATA): cv.hex_uint8_t})


@register_binary_sensor("roomba", RoombaBinarySensor, ROOMBA_SCHEMA)
def roomba_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                RoombaData,
                ("data", config[CONF_DATA]),
            )
        )
    )


@register_trigger("roomba", RoombaTrigger, RoombaData)
def roomba_trigger(var, config):
    pass


@register_dumper("roomba", RoombaDumper)
def roomba_dumper(var, config):
    pass


register_protocol_action(
    "roomba",
    "Roomba",
    ROOMBA_SCHEMA,
    (CONF_DATA, "data", cg.uint8),
)


# Sony
SonyData, SonyBinarySensor, SonyTrigger, _, SonyDumper = declare_protocol("Sony")
SONY_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DATA): cv.hex_uint32_t,
        cv.Optional(CONF_NBITS, default=12): cv.one_of(12, 15, 20, int=True),
    }
)


@register_binary_sensor("sony", SonyBinarySensor, SONY_SCHEMA)
def sony_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                SonyData,
                ("data", config[CONF_DATA]),
                ("nbits", config[CONF_NBITS]),
            )
        )
    )


@register_trigger("sony", SonyTrigger, SonyData)
def sony_trigger(var, config):
    pass


@register_dumper("sony", SonyDumper)
def sony_dumper(var, config):
    pass


register_protocol_action(
    "sony",
    "Sony",
    SONY_SCHEMA,
    (CONF_DATA, "data", cg.uint32),
    (CONF_NBITS, "nbits", cg.uint8),
)


# Symphony
SymphonyData, SymphonyBinarySensor, SymphonyTrigger, _, SymphonyDumper = (
    declare_protocol("Symphony")
)
SYMPHONY_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DATA): cv.hex_uint32_t,
        cv.Required(CONF_NBITS): cv.int_range(min=1, max=32),
        cv.Optional(CONF_COMMAND_REPEATS, default=2): cv.uint8_t,
    }
)


@register_binary_sensor("symphony", SymphonyBinarySensor, SYMPHONY_SCHEMA)
def symphony_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                SymphonyData,
                ("data", config[CONF_DATA]),
                ("nbits", config[CONF_NBITS]),
            )
        )
    )


@register_trigger("symphony", SymphonyTrigger, SymphonyData)
def symphony_trigger(var, config):
    pass


@register_dumper("symphony", SymphonyDumper)
def symphony_dumper(var, config):
    pass


register_protocol_action(
    "symphony",
    "Symphony",
    SYMPHONY_SCHEMA,
    (CONF_DATA, "data", cg.uint32),
    (CONF_NBITS, "nbits", cg.uint8),
    (CONF_COMMAND_REPEATS, "repeats", cg.uint8),
)


# Raw
def validate_raw_alternating(value):
    assert isinstance(value, list)
    last_negative = None
    for i, val in enumerate(value):
        this_negative = val < 0
        if i != 0 and this_negative == last_negative:
            raise cv.Invalid(
                f"Values must alternate between being positive and negative, please see index {i - 1} and {i}",
                [i],
            )
        last_negative = this_negative
    return value


RawData, RawBinarySensor, RawTrigger, RawAction, RawDumper = declare_protocol("Raw")
CONF_CODE_STORAGE_ID = "code_storage_id"
RAW_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CODE): cv.All(
            [cv.Any(cv.int_, cv.time_period_microseconds)],
            cv.Length(min=1),
            validate_raw_alternating,
        ),
        cv.GenerateID(CONF_CODE_STORAGE_ID): cv.declare_id(cg.int32),
    }
)


@register_binary_sensor("raw", RawBinarySensor, RAW_SCHEMA)
def raw_binary_sensor(var, config):
    code_ = config[CONF_CODE]
    arr = cg.progmem_array(config[CONF_CODE_STORAGE_ID], code_)
    cg.add(var.set_data(arr))
    cg.add(var.set_len(len(code_)))


@register_trigger("raw", RawTrigger, cg.std_vector.template(cg.int32))
def raw_trigger(var, config):
    pass


@register_dumper("raw", RawDumper)
def raw_dumper(var, config):
    pass


@register_action(
    "raw",
    RawAction,
    RAW_SCHEMA.extend(
        {
            cv.Optional(CONF_CARRIER_FREQUENCY, default="0Hz"): cv.All(
                cv.frequency, cv.int_
            ),
        }
    ),
)
async def raw_action(var, config, args):
    code_ = config[CONF_CODE]
    if cg.is_template(code_):
        template_ = await cg.templatable(code_, args, cg.std_vector.template(cg.int32))
        cg.add(var.set_code_template(template_))
    else:
        code_ = config[CONF_CODE]
        arr = cg.progmem_array(config[CONF_CODE_STORAGE_ID], code_)
        cg.add(var.set_code_static(arr, len(code_)))
    templ = await cg.templatable(config[CONF_CARRIER_FREQUENCY], args, cg.uint32)
    cg.add(var.set_carrier_frequency(templ))


# Drayton
(
    DraytonData,
    DraytonBinarySensor,
    DraytonTrigger,
    _,
    DraytonDumper,
) = declare_protocol("Drayton")
DRAYTON_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
        cv.Optional(CONF_DATA): cv.hex_int_range(min=0, max=0xF),
        cv.Optional(CONF_CHANNEL): cv.hex_int_range(min=0, max=0x1F),
        cv.Optional(CONF_COMMAND): cv.hex_int_range(min=0, max=0x7F),
    }
)
# The legacy command is the 4 data bits followed by this many checksum bits
DRAYTON_NBITS_CHECKSUM_IN_COMMAND = 3
# Either `data`, or the legacy `channel` + `command` pair (raw packet bits)
_validate_drayton_form = cv.All(
    cv.has_none_or_all_keys(CONF_CHANNEL, CONF_COMMAND),
    cv.has_exactly_one_key(CONF_DATA, CONF_COMMAND),
)


@register_binary_sensor(
    "drayton", DraytonBinarySensor, cv.All(DRAYTON_SCHEMA, _validate_drayton_form)
)
def drayton_binary_sensor(var, config):
    # Matching compares address and data only
    if (command := config.get(CONF_COMMAND)) is not None:
        data = command >> DRAYTON_NBITS_CHECKSUM_IN_COMMAND
    else:
        data = config[CONF_DATA]
    cg.add(
        var.set_data(
            cg.StructInitializer(
                DraytonData, ("address", config[CONF_ADDRESS]), ("data", data)
            )
        )
    )


@register_trigger("drayton", DraytonTrigger, DraytonData)
def drayton_trigger(var, config):
    pass


@register_dumper("drayton", DraytonDumper)
def drayton_dumper(var, config):
    pass


register_protocol_action(
    "drayton",
    "Drayton",
    DRAYTON_SCHEMA,
    (CONF_ADDRESS, "address", cg.uint16),
    (CONF_DATA, "data", cg.uint8),
    (CONF_CHANNEL, "channel", cg.uint8),
    (CONF_COMMAND, "command", cg.uint8),
    validate=_validate_drayton_form,
)


# RC5
RC5Data, RC5BinarySensor, RC5Trigger, _, RC5Dumper = declare_protocol("RC5")
RC5_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.All(cv.hex_int, cv.Range(min=0, max=0x1F)),
        cv.Required(CONF_COMMAND): cv.All(cv.hex_int, cv.Range(min=0, max=0x7F)),
    }
)


@register_binary_sensor("rc5", RC5BinarySensor, RC5_SCHEMA)
def rc5_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                RC5Data,
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("rc5", RC5Trigger, RC5Data)
def rc5_trigger(var, config):
    pass


@register_dumper("rc5", RC5Dumper)
def rc5_dumper(var, config):
    pass


register_protocol_action(
    "rc5",
    "RC5",
    RC5_SCHEMA,
    (CONF_ADDRESS, "address", cg.uint8),
    (CONF_COMMAND, "command", cg.uint8),
)


# RC5X (aka RC5 Marantz)
RC5XData, RC5XBinarySensor, RC5XTrigger, _, RC5XDumper = declare_protocol("RC5X")
RC5X_SCHEMA = RC5_SCHEMA.extend(
    {cv.Required(CONF_EXTENSION): cv.hex_int_range(min=0, max=0x3F)}
)


@register_binary_sensor("rc5x", RC5XBinarySensor, RC5X_SCHEMA)
def rc5x_binary_sensor(var: MockObj, config: ConfigType) -> None:
    cg.add(
        var.set_data(
            cg.StructInitializer(
                RC5XData,
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
                ("extension", config[CONF_EXTENSION]),
            )
        )
    )


@register_trigger("rc5x", RC5XTrigger, RC5XData)
def rc5x_trigger(var: MockObj, config: ConfigType) -> None:
    """The trigger takes no options beyond the automation."""


@register_dumper("rc5x", RC5XDumper)
def rc5x_dumper(var: MockObj, config: ConfigType) -> None:
    """The dumper takes no options."""


register_protocol_action(
    "rc5x",
    "RC5X",
    RC5X_SCHEMA,
    (CONF_ADDRESS, "address", cg.uint8),
    (CONF_COMMAND, "command", cg.uint8),
    (CONF_EXTENSION, "extension", cg.uint8),
)


# RC6
RC6Data, RC6BinarySensor, RC6Trigger, _, RC6Dumper = declare_protocol("RC6")
RC6_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.hex_uint8_t,
        cv.Required(CONF_COMMAND): cv.hex_uint8_t,
    }
)


@register_binary_sensor("rc6", RC6BinarySensor, RC6_SCHEMA)
def rc6_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                RC6Data,
                ("mode", 0),
                ("toggle", 0),
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("rc6", RC6Trigger, RC6Data)
def rc6_trigger(var, config):
    pass


@register_dumper("rc6", RC6Dumper)
def rc6_dumper(var, config):
    pass


register_protocol_action(
    "rc6",
    "RC6",
    RC6_SCHEMA,
    (CONF_ADDRESS, "address", cg.uint8),
    (CONF_COMMAND, "command", cg.uint8),
    # Each action flips its own toggle bit, starting at 0: the lambda's static is per action
    automation.ApplyCall(
        "data.toggle = []() {{ static uint8_t toggle; return toggle++; }}()"
    ),
)


# RC Switch Raw
RC_SWITCH_TIMING_SCHEMA = cv.All([cv.uint8_t], cv.Length(min=2, max=2))

RC_SWITCH_PROTOCOL_SCHEMA = cv.Any(
    cv.int_range(min=1, max=8),
    cv.Schema(
        {
            cv.Required(CONF_PULSE_LENGTH): cv.uint32_t,
            cv.Optional(CONF_SYNC, default=[1, 31]): RC_SWITCH_TIMING_SCHEMA,
            cv.Optional(CONF_ZERO, default=[1, 3]): RC_SWITCH_TIMING_SCHEMA,
            cv.Optional(CONF_ONE, default=[3, 1]): RC_SWITCH_TIMING_SCHEMA,
            cv.Optional(CONF_INVERTED, default=False): cv.boolean,
        }
    ),
)


def validate_rc_switch_code(value):
    if not isinstance(value, str):
        raise cv.Invalid("All RCSwitch codes must be in quotes ('')")
    for c in value:
        if c not in ("0", "1"):
            raise cv.Invalid(
                f"Invalid RCSwitch code character '{c}'. Only '0' and '1' are allowed"
            )
    if len(value) > 64:
        raise cv.Invalid(
            f"Maximum length for RCSwitch codes is 64, code '{value}' has length {len(value)}"
        )
    if not value:
        raise cv.Invalid("RCSwitch code must not be empty")
    return value


def validate_rc_switch_raw_code(value):
    if not isinstance(value, str):
        raise cv.Invalid("All RCSwitch raw codes must be in quotes ('')")
    for c in value:
        if c not in ("0", "1", "x"):
            raise cv.Invalid(
                f"Invalid RCSwitch raw code character '{c}'.Only '0', '1' and 'x' are allowed"
            )
    if len(value) > 64:
        raise cv.Invalid(
            f"Maximum length for RCSwitch raw codes is 64, code '{value}' has length {len(value)}"
        )
    if not value:
        raise cv.Invalid("RCSwitch raw code must not be empty")
    return value


def build_custom_rc_switch_protocol(config: ConfigType) -> MockObj:
    pl = config[CONF_PULSE_LENGTH]
    return RCSwitchBase(
        config[CONF_SYNC][0] * pl,
        config[CONF_SYNC][1] * pl,
        config[CONF_ZERO][0] * pl,
        config[CONF_ZERO][1] * pl,
        config[CONF_ONE][0] * pl,
        config[CONF_ONE][1] * pl,
        config[CONF_INVERTED],
    )


def rc_switch_protocol_in_flash(config: int | ConfigType) -> MockObj:
    """Pointer to the protocol in flash: a built-in table entry or a shared custom table."""
    if isinstance(config, int):
        return cg.RawExpression(f"&{RC_SWITCH_PROTOCOLS}[{config}]")
    return cg.shared_progmem_array(
        "rc_switch_custom_protocol",
        RCSwitchBase,
        [build_custom_rc_switch_protocol(config)],
    )


def rc_switch_protocol_value(config: int | ConfigType) -> MockObj:
    """RAM copy of a constant protocol for the transmit actions, read from its flash table."""
    if isinstance(config, int):
        return rc_switch_protocol(config)
    return rc_switch_protocol_copy(rc_switch_protocol_in_flash(config))


RC_SWITCH_RAW_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CODE): validate_rc_switch_raw_code,
        cv.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
    }
)
RC_SWITCH_TYPE_A_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_GROUP): cv.All(
            validate_rc_switch_code, cv.Length(min=5, max=5)
        ),
        cv.Required(CONF_DEVICE): cv.All(
            validate_rc_switch_code, cv.Length(min=5, max=5)
        ),
        cv.Required(CONF_STATE): cv.boolean,
        cv.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
    }
)
RC_SWITCH_TYPE_B_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.int_range(min=1, max=4),
        cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=4),
        cv.Required(CONF_STATE): cv.boolean,
        cv.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
    }
)
RC_SWITCH_TYPE_C_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_FAMILY): cv.one_of(
            "a",
            "b",
            "c",
            "d",
            "e",
            "f",
            "g",
            "h",
            "i",
            "j",
            "k",
            "l",
            "m",
            "n",
            "o",
            "p",
            lower=True,
        ),
        cv.Required(CONF_GROUP): cv.int_range(min=1, max=4),
        cv.Required(CONF_DEVICE): cv.int_range(min=1, max=4),
        cv.Required(CONF_STATE): cv.boolean,
        cv.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
    }
)
RC_SWITCH_TYPE_D_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_GROUP): cv.one_of("a", "b", "c", "d", lower=True),
        cv.Required(CONF_DEVICE): cv.int_range(min=1, max=3),
        cv.Required(CONF_STATE): cv.boolean,
        cv.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
    }
)
RC_SWITCH_TRANSMITTER = cv.Schema(
    {
        cv.Optional(CONF_REPEAT, default={CONF_TIMES: 5}): cv.Schema(
            {
                cv.Required(CONF_TIMES): cv.templatable(cv.positive_int),
                cv.Optional(CONF_WAIT_TIME, default="0us"): cv.templatable(
                    cv.positive_time_period_microseconds
                ),
            }
        ),
    }
)

rc_switch_protocol = ns.rc_switch_protocol
rc_switch_protocol_copy = ns.rc_switch_protocol_copy
RC_SWITCH_PROTOCOLS = ns.RC_SWITCH_PROTOCOLS
RCSwitchData = ns.struct("RCSwitchData")
RCSwitchBase = ns.class_("RCSwitchBase")
RCSwitchTrigger = ns.class_("RCSwitchTrigger", RemoteReceiverTrigger)
RCSwitchDumper = ns.class_("RCSwitchDumper", RemoteReceiverDumperBase)
RCSwitchRawAction = ns.class_("RCSwitchRawAction", RemoteTransmitterActionBase)
RCSwitchTypeAAction = ns.class_("RCSwitchTypeAAction", RemoteTransmitterActionBase)
RCSwitchTypeBAction = ns.class_("RCSwitchTypeBAction", RemoteTransmitterActionBase)
RCSwitchTypeCAction = ns.class_("RCSwitchTypeCAction", RemoteTransmitterActionBase)
RCSwitchTypeDAction = ns.class_("RCSwitchTypeDAction", RemoteTransmitterActionBase)
RCSwitchRawReceiver = ns.class_("RCSwitchRawReceiver", RemoteReceiverBinarySensorBase)


@register_binary_sensor("rc_switch_raw", RCSwitchRawReceiver, RC_SWITCH_RAW_SCHEMA)
def rc_switch_raw_binary_sensor(var, config):
    cg.add(var.set_protocol(rc_switch_protocol_in_flash(config[CONF_PROTOCOL])))
    cg.add(var.set_code(config[CONF_CODE]))


@register_action(
    "rc_switch_raw",
    RCSwitchRawAction,
    RC_SWITCH_RAW_SCHEMA.extend(RC_SWITCH_TRANSMITTER),
)
async def rc_switch_raw_action(var, config, args):
    proto = await cg.templatable(
        config[CONF_PROTOCOL], args, RCSwitchBase, to_exp=rc_switch_protocol_value
    )
    cg.add(var.set_protocol(proto))
    cg.add(var.set_code(await cg.templatable(config[CONF_CODE], args, cg.std_string)))


@register_binary_sensor(
    "rc_switch_type_a", RCSwitchRawReceiver, RC_SWITCH_TYPE_A_SCHEMA
)
def rc_switch_type_a_binary_sensor(var, config):
    cg.add(var.set_protocol(rc_switch_protocol_in_flash(config[CONF_PROTOCOL])))
    cg.add(var.set_type_a(config[CONF_GROUP], config[CONF_DEVICE], config[CONF_STATE]))


@register_action(
    "rc_switch_type_a",
    RCSwitchTypeAAction,
    RC_SWITCH_TYPE_A_SCHEMA.extend(RC_SWITCH_TRANSMITTER),
)
async def rc_switch_type_a_action(var, config, args):
    proto = await cg.templatable(
        config[CONF_PROTOCOL], args, RCSwitchBase, to_exp=rc_switch_protocol_value
    )
    cg.add(var.set_protocol(proto))
    cg.add(var.set_group(await cg.templatable(config[CONF_GROUP], args, cg.std_string)))
    cg.add(
        var.set_device(await cg.templatable(config[CONF_DEVICE], args, cg.std_string))
    )
    cg.add(var.set_state(await cg.templatable(config[CONF_STATE], args, cg.bool_)))


@register_binary_sensor(
    "rc_switch_type_b", RCSwitchRawReceiver, RC_SWITCH_TYPE_B_SCHEMA
)
def rc_switch_type_b_binary_sensor(var, config):
    cg.add(var.set_protocol(rc_switch_protocol_in_flash(config[CONF_PROTOCOL])))
    cg.add(
        var.set_type_b(config[CONF_ADDRESS], config[CONF_CHANNEL], config[CONF_STATE])
    )


@register_action(
    "rc_switch_type_b",
    RCSwitchTypeBAction,
    RC_SWITCH_TYPE_B_SCHEMA.extend(RC_SWITCH_TRANSMITTER),
)
async def rc_switch_type_b_action(var, config, args):
    proto = await cg.templatable(
        config[CONF_PROTOCOL], args, RCSwitchBase, to_exp=rc_switch_protocol_value
    )
    cg.add(var.set_protocol(proto))
    cg.add(var.set_address(await cg.templatable(config[CONF_ADDRESS], args, cg.uint8)))
    cg.add(var.set_channel(await cg.templatable(config[CONF_CHANNEL], args, cg.uint8)))
    cg.add(var.set_state(await cg.templatable(config[CONF_STATE], args, cg.bool_)))


@register_binary_sensor(
    "rc_switch_type_c", RCSwitchRawReceiver, RC_SWITCH_TYPE_C_SCHEMA
)
def rc_switch_type_c_binary_sensor(var, config):
    cg.add(var.set_protocol(rc_switch_protocol_in_flash(config[CONF_PROTOCOL])))
    cg.add(
        var.set_type_c(
            config[CONF_FAMILY],
            config[CONF_GROUP],
            config[CONF_DEVICE],
            config[CONF_STATE],
        )
    )


@register_action(
    "rc_switch_type_c",
    RCSwitchTypeCAction,
    RC_SWITCH_TYPE_C_SCHEMA.extend(RC_SWITCH_TRANSMITTER),
)
async def rc_switch_type_c_action(var, config, args):
    proto = await cg.templatable(
        config[CONF_PROTOCOL], args, RCSwitchBase, to_exp=rc_switch_protocol_value
    )
    cg.add(var.set_protocol(proto))
    cg.add(
        var.set_family(await cg.templatable(config[CONF_FAMILY], args, cg.std_string))
    )
    cg.add(var.set_group(await cg.templatable(config[CONF_GROUP], args, cg.uint8)))
    cg.add(var.set_device(await cg.templatable(config[CONF_DEVICE], args, cg.uint8)))
    cg.add(var.set_state(await cg.templatable(config[CONF_STATE], args, cg.bool_)))


@register_binary_sensor(
    "rc_switch_type_d",
    RCSwitchRawReceiver,
    RC_SWITCH_TYPE_D_SCHEMA.extend(RC_SWITCH_TRANSMITTER),
)
def rc_switch_type_d_binary_sensor(var, config):
    cg.add(var.set_protocol(rc_switch_protocol_in_flash(config[CONF_PROTOCOL])))
    cg.add(var.set_type_d(config[CONF_GROUP], config[CONF_DEVICE], config[CONF_STATE]))


@register_action(
    "rc_switch_type_d",
    RCSwitchTypeDAction,
    RC_SWITCH_TYPE_D_SCHEMA.extend(RC_SWITCH_TRANSMITTER),
)
async def rc_switch_type_d_action(var, config, args):
    proto = await cg.templatable(
        config[CONF_PROTOCOL], args, RCSwitchBase, to_exp=rc_switch_protocol_value
    )
    cg.add(var.set_protocol(proto))
    cg.add(var.set_group(await cg.templatable(config[CONF_GROUP], args, cg.std_string)))
    cg.add(var.set_device(await cg.templatable(config[CONF_DEVICE], args, cg.uint8)))
    cg.add(var.set_state(await cg.templatable(config[CONF_STATE], args, cg.bool_)))


@register_trigger("rc_switch", RCSwitchTrigger, RCSwitchData)
def rc_switch_trigger(var, config):
    pass


@register_dumper("rc_switch", RCSwitchDumper)
def rc_switch_dumper(var, config):
    pass


# Samsung
(
    SamsungData,
    SamsungBinarySensor,
    SamsungTrigger,
    _,
    SamsungDumper,
) = declare_protocol("Samsung")
SAMSUNG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DATA): cv.hex_uint64_t,
        cv.Optional(CONF_NBITS, default=32): cv.int_range(32, 64),
    }
)


@register_binary_sensor("samsung", SamsungBinarySensor, SAMSUNG_SCHEMA)
def samsung_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                SamsungData,
                ("data", config[CONF_DATA]),
                ("nbits", config[CONF_NBITS]),
            )
        )
    )


@register_trigger("samsung", SamsungTrigger, SamsungData)
def samsung_trigger(var, config):
    pass


@register_dumper("samsung", SamsungDumper)
def samsung_dumper(var, config):
    pass


register_protocol_action(
    "samsung",
    "Samsung",
    SAMSUNG_SCHEMA,
    (CONF_DATA, "data", cg.uint64),
    (CONF_NBITS, "nbits", cg.uint8),
)


# Samsung36
(
    Samsung36Data,
    Samsung36BinarySensor,
    Samsung36Trigger,
    _,
    Samsung36Dumper,
) = declare_protocol("Samsung36")
SAMSUNG36_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
        cv.Required(CONF_COMMAND): cv.hex_uint32_t,
    }
)


@register_binary_sensor("samsung36", Samsung36BinarySensor, SAMSUNG36_SCHEMA)
def samsung36_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                Samsung36Data,
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("samsung36", Samsung36Trigger, Samsung36Data)
def samsung36_trigger(var, config):
    pass


@register_dumper("samsung36", Samsung36Dumper)
def samsung36_dumper(var, config):
    pass


register_protocol_action(
    "samsung36",
    "Samsung36",
    SAMSUNG36_SCHEMA,
    (CONF_ADDRESS, "address", cg.uint16),
    (CONF_COMMAND, "command", cg.uint32),
)


# Toshiba AC
(
    ToshibaAcData,
    ToshibaAcBinarySensor,
    ToshibaAcTrigger,
    _,
    ToshibaAcDumper,
) = declare_protocol("ToshibaAc")
TOSHIBAAC_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_RC_CODE_1): cv.hex_uint64_t,
        cv.Optional(CONF_RC_CODE_2, default=0): cv.hex_uint64_t,
    }
)


@register_binary_sensor("toshiba_ac", ToshibaAcBinarySensor, TOSHIBAAC_SCHEMA)
def toshibaac_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                ToshibaAcData,
                ("rc_code_1", config[CONF_RC_CODE_1]),
                ("rc_code_2", config[CONF_RC_CODE_2]),
            )
        )
    )


@register_trigger("toshiba_ac", ToshibaAcTrigger, ToshibaAcData)
def toshibaac_trigger(var, config):
    pass


@register_dumper("toshiba_ac", ToshibaAcDumper)
def toshibaac_dumper(var, config):
    pass


register_protocol_action(
    "toshiba_ac",
    "ToshibaAc",
    TOSHIBAAC_SCHEMA,
    (CONF_RC_CODE_1, "rc_code_1", cg.uint64),
    (CONF_RC_CODE_2, "rc_code_2", cg.uint64),
)


# Panasonic
(
    PanasonicData,
    PanasonicBinarySensor,
    PanasonicTrigger,
    _,
    PanasonicDumper,
) = declare_protocol("Panasonic")
PANASONIC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_NBITS, default=48): cv.one_of(48, 56, int=True),
        cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
        cv.Optional(CONF_ADDRESS_2, default=0x00): cv.hex_uint8_t,
        cv.Required(CONF_COMMAND): cv.hex_uint32_t,
    }
)


def _panasonic_address_2_needs_56_bits(config: ConfigType) -> ConfigType:
    # A 48-bit code has no second address byte; a lambda is only known on the device
    address_2 = config[CONF_ADDRESS_2]
    if config[CONF_NBITS] == 48 and isinstance(address_2, int) and address_2 != 0:
        raise cv.Invalid(
            f"{CONF_ADDRESS_2} is only sent with {CONF_NBITS}: 56", [CONF_ADDRESS_2]
        )
    return config


# Transmit only
PANASONIC_ACTION_SCHEMA = PANASONIC_SCHEMA.extend(
    {
        cv.Optional(CONF_CARRIER_FREQUENCY, default="35000Hz"): cv.All(
            cv.frequency, cv.uint32_t
        ),
    }
)


@register_binary_sensor(
    "panasonic",
    PanasonicBinarySensor,
    cv.All(PANASONIC_SCHEMA, _panasonic_address_2_needs_56_bits),
)
def panasonic_binary_sensor(var: MockObj, config: ConfigType) -> None:
    cg.add(
        var.set_data(
            cg.StructInitializer(
                PanasonicData,
                ("address", config[CONF_ADDRESS]),
                ("command", config[CONF_COMMAND]),
                ("nbits", config[CONF_NBITS]),
                ("address_2", config[CONF_ADDRESS_2]),
            )
        )
    )


@register_trigger("panasonic", PanasonicTrigger, PanasonicData)
def panasonic_trigger(var: MockObj, config: ConfigType) -> None:
    """The trigger takes no options beyond the automation."""


@register_dumper("panasonic", PanasonicDumper)
def panasonic_dumper(var: MockObj, config: ConfigType) -> None:
    """The dumper takes no options."""


register_protocol_action(
    "panasonic",
    "Panasonic",
    PANASONIC_ACTION_SCHEMA,
    (CONF_NBITS, "nbits", cg.uint8),
    (CONF_ADDRESS, "address", cg.uint16),
    (CONF_ADDRESS_2, "address_2", cg.uint8),
    (CONF_COMMAND, "command", cg.uint32),
    (CONF_CARRIER_FREQUENCY, "carrier_frequency", cg.uint32),
    validate=_panasonic_address_2_needs_56_bits,
)


# SomfyRts
SOMFY_RTS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_KEY): cv.hex_uint8_t,
        cv.Required(CONF_CONTROL): cv.All(cv.hex_uint8_t, cv.Range(max=0x0F)),
        cv.Required(CONF_ROLLING_CODE): cv.hex_uint16_t,
        cv.Required(CONF_ADDRESS): cv.All(cv.hex_uint32_t, cv.Range(max=0xFFFFFF)),
        cv.Optional(CONF_CONTROL_IN_KEY, default=False): cv.boolean,
    }
)


def _validate_somfy_rts(config: ConfigType) -> ConfigType:
    key = config[CONF_KEY]
    if config[CONF_CONTROL_IN_KEY] is True and isinstance(key, int) and key & 0x0F:
        raise cv.Invalid(
            f"With {CONF_CONTROL_IN_KEY} the low nibble of {CONF_KEY} carries {CONF_CONTROL} "
            "and must be 0",
            path=[CONF_KEY],
        )
    return config


# Transmit only: the protocol has no decoder yet, so no binary sensor, trigger or dumper.
register_protocol_action(
    "somfy_rts",
    "SomfyRts",
    SOMFY_RTS_SCHEMA,
    (CONF_KEY, "key", cg.uint8),
    (CONF_CONTROL, "control", cg.uint8),
    (CONF_ROLLING_CODE, "rolling_code", cg.uint16),
    (CONF_ADDRESS, "address", cg.uint32),
    (CONF_CONTROL_IN_KEY, "control_in_key", cg.bool_),
    validate=_validate_somfy_rts,
)

# Nexa
NexaData, NexaBinarySensor, NexaTrigger, _, NexaDumper = declare_protocol("Nexa")
NEXA_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_DEVICE): cv.hex_uint32_t,
        cv.Required(CONF_GROUP): cv.hex_uint8_t,
        cv.Required(CONF_STATE): cv.hex_uint8_t,
        cv.Required(CONF_CHANNEL): cv.hex_uint8_t,
        cv.Required(CONF_LEVEL): cv.hex_uint8_t,
    }
)


@register_binary_sensor("nexa", NexaBinarySensor, NEXA_SCHEMA)
def nexa_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                NexaData,
                ("device", config[CONF_DEVICE]),
                ("group", config[CONF_GROUP]),
                ("state", config[CONF_STATE]),
                ("channel", config[CONF_CHANNEL]),
                ("level", config[CONF_LEVEL]),
            )
        )
    )


@register_trigger("nexa", NexaTrigger, NexaData)
def nexa_trigger(var, config):
    pass


@register_dumper("nexa", NexaDumper)
def nexa_dumper(var, config):
    pass


register_protocol_action(
    "nexa",
    "Nexa",
    NEXA_SCHEMA,
    (CONF_DEVICE, "device", cg.uint32),
    (CONF_GROUP, "group", cg.uint8),
    (CONF_STATE, "state", cg.uint8),
    (CONF_CHANNEL, "channel", cg.uint8),
    (CONF_LEVEL, "level", cg.uint8),
)


# Nexus
NexusData, NexusBinarySensor, NexusTrigger, _, NexusDumper = declare_protocol("Nexus")
NEXUS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=4),
        cv.Required(CONF_ADDRESS): cv.hex_uint8_t,
    }
)
NEXUS_ACTION_SCHEMA = NEXUS_SCHEMA.extend(
    {
        cv.Required(CONF_TEMPERATURE): cv.All(
            cv.float_, cv.Range(min=-204.8, max=204.7)
        ),
        cv.Required(CONF_HUMIDITY): cv.uint8_t,
        cv.Optional(CONF_BATTERY_LEVEL, default=True): cv.boolean,
        cv.Optional(CONF_FORCE_UPDATE, default=False): cv.boolean,
    }
)


@register_binary_sensor("nexus", NexusBinarySensor, NEXUS_SCHEMA)
def nexus_binary_sensor(var: MockObj, config: ConfigType) -> None:
    cg.add(
        var.set_data(
            cg.StructInitializer(
                NexusData,
                ("address", config[CONF_ADDRESS]),
                ("channel", config[CONF_CHANNEL]),
            )
        )
    )


@register_trigger("nexus", NexusTrigger, NexusData)
def nexus_trigger(var: MockObj, config: ConfigType) -> None:
    """The trigger takes no options beyond the automation."""


@register_dumper("nexus", NexusDumper)
def nexus_dumper(var: MockObj, config: ConfigType) -> None:
    """The dumper takes no options."""


register_protocol_action(
    "nexus",
    "Nexus",
    NEXUS_ACTION_SCHEMA,
    (CONF_CHANNEL, "channel", cg.uint8),
    (CONF_ADDRESS, "address", cg.uint8),
    (CONF_TEMPERATURE, "temperature", cg.float_),
    (CONF_HUMIDITY, "humidity", cg.uint8),
    (CONF_BATTERY_LEVEL, "battery_level", cg.bool_),
    (CONF_FORCE_UPDATE, "force_update", cg.bool_),
)


# PD Pioneer
(
    PDPioneerData,
    PDPioneerBinarySensor,
    PDPioneerTrigger,
    _,
    PDPioneerDumper,
) = declare_protocol("PDPioneer")


def _validate_pd_pioneer_burst(code: list[int]) -> list[int]:
    """Byte 3 tells the odd (0x02) and even (0x01) bursts apart; decode() drops anything else."""
    if code[3] not in (0x01, 0x02):
        raise cv.Invalid("Byte 3 of a pd_pioneer code must be 0x01 or 0x02")
    return code


PDPIONEER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CODE): cv.All(
            [cv.hex_uint8_t], cv.Length(min=13, max=14), _validate_pd_pioneer_burst
        ),
    }
)


def _pd_pioneer_finalized(code: list[int]) -> list[int]:
    """Fill in the checksum: the sum of the first 13 bytes, plus 15 for an odd burst."""
    checksum = sum(code[:13], 15 if code[3] == 0x02 else 0) & 0xFF
    return [*code[:13], checksum]


@register_binary_sensor("pd_pioneer", PDPioneerBinarySensor, PDPIONEER_SCHEMA)
def pd_pioneer_binary_sensor(var: MockObj, config: ConfigType) -> None:
    cg.add(var.set_data(_pd_pioneer_finalized(config[CONF_CODE])))


@register_trigger("pd_pioneer", PDPioneerTrigger, PDPioneerData)
def pd_pioneer_trigger(var: MockObj, config: ConfigType) -> None:
    """The trigger takes no options beyond the automation."""


@register_dumper("pd_pioneer", PDPioneerDumper)
def pd_pioneer_dumper(var: MockObj, config: ConfigType) -> None:
    """The dumper takes no options."""


register_protocol_action(
    "pd_pioneer",
    "PDPioneer",
    PDPIONEER_SCHEMA,
    _bytes_field(CONF_CODE, f"data = {PDPioneerData}({{}})"),
    automation.ApplyCall("data.finalize()"),
)


# Midea
MideaData, MideaBinarySensor, MideaTrigger, _, MideaDumper = declare_protocol("Midea")
MIDEA_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CODE): cv.All([cv.hex_uint8_t], cv.Length(min=5, max=5)),
    }
)


@register_binary_sensor("midea", MideaBinarySensor, MIDEA_SCHEMA)
def midea_binary_sensor(var, config):
    cg.add(var.set_data(config[CONF_CODE]))


@register_trigger("midea", MideaTrigger, MideaData)
def midea_trigger(var, config):
    pass


@register_dumper("midea", MideaDumper)
def midea_dumper(var, config):
    pass


register_protocol_action(
    "midea",
    "Midea",
    MIDEA_SCHEMA,
    _bytes_field(CONF_CODE, f"data = {MideaData}({{}})"),
    automation.ApplyCall("data.finalize()"),
)


# AEHA
AEHAData, AEHABinarySensor, AEHATrigger, _, AEHADumper = declare_protocol("AEHA")
AEHA_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
        cv.Required(CONF_DATA): cv.All([cv.hex_uint8_t], cv.Length(min=2, max=35)),
    }
)


@register_binary_sensor("aeha", AEHABinarySensor, AEHA_SCHEMA)
def aeha_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                AEHAData,
                ("address", config[CONF_ADDRESS]),
                ("data", config[CONF_DATA]),
            )
        )
    )


@register_trigger("aeha", AEHATrigger, AEHAData)
def aeha_trigger(var, config):
    pass


@register_dumper("aeha", AEHADumper)
def aeha_dumper(var, config):
    pass


register_protocol_action(
    "aeha",
    "AEHA",
    AEHA_SCHEMA.extend(
        {
            cv.Optional(CONF_CARRIER_FREQUENCY, default="38000Hz"): cv.All(
                cv.frequency, cv.int_
            ),
        }
    ),
    automation.ApplyField(
        CONF_CARRIER_FREQUENCY, "get_data()->set_carrier_frequency({})", cg.uint32
    ),
    automation.ApplyCall(
        "encode({}, {})",
        (
            (CONF_ADDRESS, cg.uint16),
            (CONF_DATA, _BYTES, _code_table),
        ),
    ),
    call="encode_call",
)


# Hob2Hood
(
    Hob2HoodData,
    Hob2HoodBinarySensor,
    Hob2HoodTrigger,
    _,
    Hob2HoodDumper,
) = declare_protocol("Hob2Hood")

Hob2HoodCommand = remote_base_ns.enum("Hob2HoodCommand")
HOB2HOOD_COMMAND_OPTIONS = Hob2HoodCommand.options(
    "light_off",
    "light_on",
    "fan_off",
    "fan_low",
    "fan_medium",
    "fan_high",
    "fan_max",
    prefix="HOB2HOOD_COMMAND_",
)

HOB2HOOD_SCHEMA = cv.Schema(
    {cv.Required(CONF_COMMAND): cv.enum(HOB2HOOD_COMMAND_OPTIONS, lower=True)}
)


@register_binary_sensor("hob2hood", Hob2HoodBinarySensor, HOB2HOOD_SCHEMA)
def hob2hood_binary_sensor(var: MockObj, config: ConfigType) -> None:
    cg.add(
        var.set_data(
            cg.StructInitializer(
                Hob2HoodData,
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("hob2hood", Hob2HoodTrigger, Hob2HoodData)
def hob2hood_trigger(var: MockObj, config: ConfigType) -> None:
    """The trigger takes no options beyond the automation."""


@register_dumper("hob2hood", Hob2HoodDumper)
def hob2hood_dumper(var: MockObj, config: ConfigType) -> None:
    """The dumper takes no options."""


register_protocol_action(
    "hob2hood",
    "Hob2Hood",
    HOB2HOOD_SCHEMA,
    (CONF_COMMAND, "command", Hob2HoodCommand),
)


# Haier
HaierData, HaierBinarySensor, HaierTrigger, _, HaierDumper = declare_protocol("Haier")
HAIER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CODE): cv.All(
            [cv.hex_uint8_t],
            cv.Any(
                cv.Length(min=8, max=8),
                cv.Length(min=13, max=13),
                msg="must be a list of length 8 or 13",
            ),
        ),
    }
)


@register_binary_sensor("haier", HaierBinarySensor, HAIER_SCHEMA)
def haier_binary_sensor(var, config):
    cg.add(var.set_code(config[CONF_CODE]))


@register_trigger("haier", HaierTrigger, HaierData)
def haier_trigger(var, config):
    pass


@register_dumper("haier", HaierDumper)
def haier_dumper(var, config):
    pass


register_protocol_action(
    "haier",
    "Haier",
    HAIER_SCHEMA,
    _bytes_field(CONF_CODE, "encode({})"),
    call="encode_call",
)


# ABBWelcome
(
    ABBWelcomeData,
    ABBWelcomeBinarySensor,
    ABBWelcomeTrigger,
    _,
    ABBWelcomeDumper,
) = declare_protocol("ABBWelcome")

CONF_SOURCE_ADDRESS = "source_address"
CONF_DESTINATION_ADDRESS = "destination_address"
CONF_THREE_BYTE_ADDRESS = "three_byte_address"
CONF_MESSAGE_TYPE = "message_type"
CONF_MESSAGE_ID = "message_id"
CONF_RETRANSMISSION = "retransmission"

ABB_WELCOME_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SOURCE_ADDRESS): cv.hex_uint32_t,
        cv.Required(CONF_DESTINATION_ADDRESS): cv.hex_uint32_t,
        cv.Optional(CONF_RETRANSMISSION, default=False): cv.boolean,
        cv.Optional(CONF_THREE_BYTE_ADDRESS, default=False): cv.boolean,
        cv.Required(CONF_MESSAGE_TYPE): cv.Any(cv.hex_uint8_t, cv.uint8_t),
        cv.Optional(CONF_MESSAGE_ID): cv.Any(cv.hex_uint8_t, cv.uint8_t),
        cv.Optional(CONF_DATA): cv.All(
            [cv.Any(cv.hex_uint8_t, cv.uint8_t)],
            cv.Length(min=0, max=7),
        ),
    }
)


@register_binary_sensor("abbwelcome", ABBWelcomeBinarySensor, ABB_WELCOME_SCHEMA)
def abbwelcome_binary_sensor(var, config):
    cg.add(var.set_three_byte_address(config[CONF_THREE_BYTE_ADDRESS]))
    cg.add(var.set_source_address(config[CONF_SOURCE_ADDRESS]))
    cg.add(var.set_destination_address(config[CONF_DESTINATION_ADDRESS]))
    cg.add(var.set_retransmission(config[CONF_RETRANSMISSION]))
    cg.add(var.set_message_type(config[CONF_MESSAGE_TYPE]))
    cg.add(var.set_auto_message_id(CONF_MESSAGE_ID not in config))
    if CONF_MESSAGE_ID in config:
        cg.add(var.set_message_id(config[CONF_MESSAGE_ID]))
    if CONF_DATA in config:
        cg.add(var.set_data(config[CONF_DATA]))
    cg.add(var.finalize())


@register_trigger("abbwelcome", ABBWelcomeTrigger, ABBWelcomeData)
def abbwelcome_trigger(var, config):
    pass


@register_dumper("abbwelcome", ABBWelcomeDumper)
def abbwelcome_dumper(var, config):
    pass


register_protocol_action(
    "abbwelcome",
    "ABBWelcome",
    ABB_WELCOME_SCHEMA,
    # First: the address length decides where the other fields go
    automation.ApplyField(
        CONF_THREE_BYTE_ADDRESS, "data.set_three_byte_address({})", cg.bool_
    ),
    automation.ApplyField(
        CONF_SOURCE_ADDRESS, "data.set_source_address({})", cg.uint32
    ),
    automation.ApplyField(
        CONF_DESTINATION_ADDRESS, "data.set_destination_address({})", cg.uint32
    ),
    automation.ApplyField(CONF_RETRANSMISSION, "data.set_retransmission({})", cg.bool_),
    automation.ApplyField(CONF_MESSAGE_TYPE, "data.set_message_type({})", cg.uint8),
    # Without message_id, finalize() picks one
    automation.ApplyCall("data.auto_message_id = true"),
    automation.ApplyField(CONF_MESSAGE_ID, "data.set_fixed_message_id({})", cg.uint8),
    _bytes_field(CONF_DATA, "data.set_data({})"),
    automation.ApplyCall("data.finalize()"),
)


# Mirage
(
    MirageData,
    MirageBinarySensor,
    MirageTrigger,
    _,
    MirageDumper,
) = declare_protocol("Mirage")

MIRAGE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CODE): cv.All([cv.hex_uint8_t], cv.Length(min=14, max=14)),
    }
)


@register_binary_sensor("mirage", MirageBinarySensor, MIRAGE_SCHEMA)
def mirage_binary_sensor(var, config):
    cg.add(var.set_code(config[CONF_CODE]))


@register_trigger("mirage", MirageTrigger, MirageData)
def mirage_trigger(var, config):
    pass


@register_dumper("mirage", MirageDumper)
def mirage_dumper(var, config):
    pass


register_protocol_action(
    "mirage",
    "Mirage",
    MIRAGE_SCHEMA,
    _bytes_field(CONF_CODE, "encode({})"),
    call="encode_call",
)


# Toto
(
    TotoData,
    TotoBinarySensor,
    TotoTrigger,
    _,
    TotoDumper,
) = declare_protocol("Toto")

TOTO_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_RC_CODE_1, default=0): cv.hex_int_range(0, 0xF),
        cv.Optional(CONF_RC_CODE_2, default=0): cv.hex_int_range(0, 0xF),
        cv.Required(CONF_COMMAND): cv.hex_uint8_t,
    }
)


@register_binary_sensor("toto", TotoBinarySensor, TOTO_SCHEMA)
def toto_binary_sensor(var, config):
    cg.add(
        var.set_data(
            cg.StructInitializer(
                TotoData,
                ("rc_code_1", config[CONF_RC_CODE_1]),
                ("rc_code_2", config[CONF_RC_CODE_2]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("toto", TotoTrigger, TotoData)
def toto_trigger(var, config):
    pass


@register_dumper("toto", TotoDumper)
def toto_dumper(var, config):
    pass


def _toto_default_repeat(config: ConfigType) -> ConfigType:
    # Toto remotes send each frame 3 times, 36 ms apart
    if CONF_REPEAT in config:
        return config
    return {**config, CONF_REPEAT: {CONF_TIMES: 3, CONF_WAIT_TIME: 36000}}


register_protocol_action(
    "toto",
    "Toto",
    TOTO_SCHEMA,
    (CONF_RC_CODE_1, "rc_code_1", cg.uint8),
    (CONF_RC_CODE_2, "rc_code_2", cg.uint8),
    (CONF_COMMAND, "command", cg.uint8),
    validate=_toto_default_repeat,
)


# IRA211
(
    IRA211Data,
    IRA211BinarySensor,
    IRA211Trigger,
    _,
    IRA211Dumper,
) = declare_protocol("IRA211")

IRA211CommandEnum = ns.enum("IRA211Command", is_class=True)
IRA211_COMMANDS = IRA211CommandEnum.options(
    "TEMP_UP", "TEMP_DOWN", "MODE", "FAN", "POWER", "SYNC", prefix="IRA211_COMMAND_"
)

IRA211ModeEnum = ns.enum("IRA211Mode", is_class=True)
IRA211_MODES = IRA211ModeEnum.options(
    "PROTECTION", "TIMER", "COMFORT", prefix="IRA211_MODE_"
)

IRA211FanEnum = ns.enum("IRA211Fan", is_class=True)
IRA211_FAN_SPEEDS = IRA211FanEnum.options(
    "AUTO", "LOW", "MEDIUM", "HIGH", prefix="IRA211_FAN_"
)


def _half_degree_step(value: float) -> float:
    if value * 2 != int(value * 2):
        raise cv.Invalid("Temperature must be a multiple of 0.5")
    return value


IRA211_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_COMMAND): cv.enum(IRA211_COMMANDS, upper=True),
        cv.Optional(CONF_TEMPERATURE, default=20): cv.All(
            cv.float_range(min=5, max=35), _half_degree_step
        ),
        cv.Optional(CONF_MODE, default="COMFORT"): cv.enum(IRA211_MODES, upper=True),
        cv.Optional(CONF_FAN_MODE, default="AUTO"): cv.enum(
            IRA211_FAN_SPEEDS, upper=True
        ),
    }
)


@register_binary_sensor("ira211", IRA211BinarySensor, IRA211_SCHEMA)
def ira211_binary_sensor(var: MockObj, config: ConfigType) -> None:
    cg.add(
        var.set_data(
            cg.StructInitializer(
                IRA211Data,
                ("command", config[CONF_COMMAND]),
                ("half_degrees", int(config[CONF_TEMPERATURE] * 2)),
                ("mode", config[CONF_MODE]),
                ("fan", config[CONF_FAN_MODE]),
            )
        )
    )


@register_trigger("ira211", IRA211Trigger, IRA211Data)
def ira211_trigger(var: MockObj, config: ConfigType) -> None:
    """The trigger takes no options beyond the automation."""


@register_dumper("ira211", IRA211Dumper)
def ira211_dumper(var: MockObj, config: ConfigType) -> None:
    """The dumper takes no options."""


register_protocol_action(
    "ira211",
    "IRA211",
    IRA211_SCHEMA,
    (CONF_COMMAND, "command", IRA211CommandEnum),
    automation.ApplyField(CONF_TEMPERATURE, "data.set_temperature({})", cg.float_),
    (CONF_MODE, "mode", IRA211ModeEnum),
    (CONF_FAN_MODE, "fan", IRA211FanEnum),
)


# A-OK
(
    AOKData,
    AOKBinarySensor,
    AOKTrigger,
    _,
    AOKDumper,
) = declare_protocol("AOK")

AOK_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_REMOTE_ID): cv.hex_int_range(0, 0xFFFFFF),
        cv.Required(CONF_CHANNELS): cv.hex_uint16_t,
        cv.Required(CONF_COMMAND): cv.hex_uint8_t,
    }
)


@register_binary_sensor("aok", AOKBinarySensor, AOK_SCHEMA)
def aok_binary_sensor(var: MockObj, config: ConfigType) -> None:
    cg.add(
        var.set_data(
            cg.StructInitializer(
                AOKData,
                ("remote_id", config[CONF_REMOTE_ID]),
                ("channels", config[CONF_CHANNELS]),
                ("command", config[CONF_COMMAND]),
            )
        )
    )


@register_trigger("aok", AOKTrigger, AOKData)
def aok_trigger(var: MockObj, config: ConfigType) -> None:
    """The trigger takes no options beyond the automation."""


@register_dumper("aok", AOKDumper)
def aok_dumper(var: MockObj, config: ConfigType) -> None:
    """The dumper takes no options."""


register_protocol_action(
    "aok",
    "AOK",
    AOK_SCHEMA,
    (CONF_REMOTE_ID, "remote_id", cg.uint32),
    (CONF_CHANNELS, "channels", cg.uint16),
    (CONF_COMMAND, "command", cg.uint8),
)
