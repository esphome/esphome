from collections.abc import Callable
from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components import modbus
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_COUNT,
    CONF_ID,
    CONF_ON_ERROR,
    CONF_ON_RESPONSE,
    CONF_VALUE,
)
from esphome.core import ID, Lambda
from esphome.types import ConfigType, TemplateArgsType

CODEOWNERS = ["@exciton"]
DEPENDENCIES = ["modbus"]
MULTI_CONF = True
# The modbus hub auto-loads this component to make the actions available. Without this, that auto-load
# would try to create a device with no address.
MULTI_CONF_NO_DEFAULT = True

CONF_ON_CUSTOM_RESPONSE = "on_custom_response"
CONF_ON_NO_RESPONSE = "on_no_response"
CONF_ON_NOT_SENT = "on_not_sent"
CONF_ON_SENT = "on_sent"
CONF_PDU = "pdu"
CONF_READ_ADDRESS = "read_address"
CONF_READ_COUNT = "read_count"
CONF_RETRY = "retry"
CONF_START_ADDRESS = "start_address"
CONF_VALUES = "values"
CONF_WRITE_ADDRESS = "write_address"

modbus_client_ns = cg.esphome_ns.namespace("modbus_client")
ModbusClientSendAction = modbus_client_ns.class_(
    "ModbusClientSendAction", automation.Action, modbus.ModbusClientDevice
)
ReadRegistersAction = modbus_client_ns.class_(
    "ReadRegistersAction", automation.Action, modbus.ModbusClientDevice
)
WriteSingleRegisterAction = modbus_client_ns.class_(
    "WriteSingleRegisterAction", automation.Action, modbus.ModbusClientDevice
)
WriteSingleCoilAction = modbus_client_ns.class_(
    "WriteSingleCoilAction", automation.Action, modbus.ModbusClientDevice
)
ReadBitsAction = modbus_client_ns.class_(
    "ReadBitsAction", automation.Action, modbus.ModbusClientDevice
)

WriteMultipleRegistersAction = modbus_client_ns.class_(
    "WriteMultipleRegistersAction", automation.Action, modbus.ModbusClientDevice
)
WriteMultipleCoilsAction = modbus_client_ns.class_(
    "WriteMultipleCoilsAction", automation.Action, modbus.ModbusClientDevice
)
ReadWriteMultipleRegistersAction = modbus_client_ns.class_(
    "ReadWriteMultipleRegistersAction", automation.Action, modbus.ModbusClientDevice
)

# Packed bit view delivered to read_coils / read_discrete_inputs on_response handlers.
PackedBits = modbus.modbus_ns.class_("PackedBits")

# The exception code passed to on_error handlers.
ExceptionCode = modbus.modbus_ns.enum("ExceptionCode")

# Lambda argument types for the reply handlers: the device address the send targeted, and the
# request/response PDUs (function code + data). The spans are only valid for the duration of the handler.
_PDU_SPAN = cg.std_span.template(cg.uint8.operator("const"))

# The pdu lambda's return type: a stack-allocated StaticVector capped at the Modbus PDU limit
# (modbus.MAX_PDU_SIZE). Lambdas can return a byte list or a modbus::helpers::create_*_pdu() result.
# The list form below is bounded by cv.Length; a lambda cannot be. PduBuffer drops bytes past
# modbus.MAX_PDU_SIZE without reporting it, so an over-long lambda PDU is silently truncated.
_PDU_BUFFER = modbus.modbus_ns.namespace("helpers").class_("PduBuffer")

# A bare modbus::ModbusClientDevice bound to a hub and a device address, and nothing else - no polling,
# no entities, no automation wiring. It exists so a lambda can talk to a device directly:
#
#   modbus_client:
#     - id: my_client
#       address: 0x01
#
#   - lambda: "id(my_client).write_single_register(0x10, 42);"
#
# Nothing here overrides the device callbacks, so every outcome takes the base class default, and those
# are no-ops: a successful reply, a Modbus exception, a timeout, and a frame that never reached the wire
# are all discarded without a log. The single exception is a reply the dispatch gate treats as
# non-standard, which warns once per device and logs at VERBOSE after that. So a lambda gets no feedback
# on an ordinary failure - use the modbus_client.* actions whenever the outcome matters, since they carry
# on_response/on_error/on_no_response/on_not_sent handlers.
# The id is required, not generated: the device is reachable only through id() in a lambda, so an
# entry without one builds something nothing can name. Better to say so than to accept dead config.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(modbus.ModbusClientDevice),
    }
).extend(modbus.modbus_device_schema(None))


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await modbus.register_modbus_client_device(var, config)


def _packed_bit_bytes(bits: int) -> int:
    """Mirrors modbus::packed_bit_bytes(): bytes needed to hold this many coils on the wire."""
    return (bits + 7) // 8


def _synchronous_handler(value: ConfigType) -> ConfigType:
    """Reject deferring actions in a handler: its PDU spans point into hub buffers that are reused
    once the handler returns, and DelayAction and friends capture the trigger args for later replay."""
    if automation.has_non_synchronous_actions(value):
        raise cv.Invalid(
            "Deferring actions (delay, wait_until, script.wait, ...) are not allowed in modbus_client "
            "handlers: the request/response data is only valid while the handler runs. Copy what you "
            "need into globals first, then defer in a separate script or automation."
        )
    return value


def _handler_schema() -> cv.All:
    return cv.All(automation.validate_automation(single=True), _synchronous_handler)


# Each action is its own hub device: the modbus hub routes the reply straight back to the action that
# sent it, so the address can even be templatable - the reply is matched by the action's identity, not
# its address.
_ACTION_BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(modbus.CONF_MODBUS_ID): cv.use_id(modbus.ModbusClient),
        cv.Required(CONF_ADDRESS): cv.templatable(cv.hex_uint8_t),
        # Optional handlers. on_sent fires when the frame reaches the wire; the reply handlers arrive
        # later (fire-and-continue), so all run with the request/reply available - not the outer
        # automation's variables.
        cv.Optional(CONF_ON_SENT): _handler_schema(),
        cv.Optional(CONF_ON_ERROR): _handler_schema(),
        # on_no_response takes either a returning lambda (`!lambda "return <bool>;"`, gets `request`,
        # returns true to have the hub retry the frame) OR a `then:` automation of actions; the automation
        # form may also carry an optional `retry:` returning lambda to run actions AND decide the retry.
        cv.Optional(CONF_ON_NO_RESPONSE): cv.All(
            cv.Any(
                cv.returning_lambda,
                automation.validate_automation(
                    {cv.Optional(CONF_RETRY): cv.returning_lambda}, single=True
                ),
            ),
            _synchronous_handler,
        ),
        cv.Optional(CONF_ON_NOT_SENT): _handler_schema(),
    }
)

MODBUS_CLIENT_SEND_SCHEMA = _ACTION_BASE_SCHEMA.extend(
    {
        cv.Required(CONF_PDU): cv.templatable(
            cv.All(
                cv.ensure_list(cv.hex_uint8_t),
                cv.Length(min=1, max=modbus.MAX_PDU_SIZE),
            )
        ),
        cv.Optional(CONF_ON_RESPONSE): _handler_schema(),
    }
)


async def register_client_action(
    var: cg.MockObj,
    config: ConfigType,
    args: TemplateArgsType,
    response_args: TemplateArgsType,
) -> cg.MockObj:
    """Wire the shared action plumbing: hub parent, templated device address, outcome triggers.

    response_args are the on_response handler's arguments, which differ per action.
    """
    parent = await cg.get_variable(config[modbus.CONF_MODBUS_ID])
    cg.add(var.set_parent(parent))
    cg.add(
        var.set_target_address(
            await cg.templatable(config[CONF_ADDRESS], args, cg.uint8)
        )
    )
    # Present for every typed action and absent from modbus_client.send, which has a pdu instead.
    if (start_address := config.get(CONF_START_ADDRESS)) is not None:
        cg.add(
            var.set_start_address(await cg.templatable(start_address, args, cg.uint16))
        )
    if sent_conf := config.get(CONF_ON_SENT):
        await automation.build_automation(
            var.get_sent_trigger(), [(_PDU_SPAN, "request")], sent_conf
        )
    if response_conf := config.get(CONF_ON_RESPONSE):
        await automation.build_automation(
            var.get_response_trigger(), response_args, response_conf
        )
    if custom_conf := config.get(CONF_ON_CUSTOM_RESPONSE):
        # Tell the action a handler exists; without this it falls back to the base's warn-once log so an
        # unhandled diverted reply is still reported instead of firing an empty trigger.
        cg.add(var.set_custom_response_handled())
        await automation.build_automation(
            var.get_custom_response_trigger(),
            [(_PDU_SPAN, "request"), (_PDU_SPAN, "response")],
            custom_conf,
        )
    if error_conf := config.get(CONF_ON_ERROR):
        await automation.build_automation(
            var.get_error_trigger(),
            [(_PDU_SPAN, "request"), (ExceptionCode, "exception_code")],
            error_conf,
        )
    if (no_response_conf := config.get(CONF_ON_NO_RESPONSE)) is not None:
        # The lambda form IS the retry decision; the automation form runs actions and may carry a nested
        # `retry:` lambda. Either way the retry lambda's bool becomes on_no_response()'s return value.
        if isinstance(no_response_conf, Lambda):
            retry_conf = no_response_conf
        else:
            await automation.build_automation(
                var.get_no_response_trigger(),
                [(_PDU_SPAN, "request")],
                no_response_conf,
            )
            retry_conf = no_response_conf.get(CONF_RETRY)
        if retry_conf is not None:
            retry_lambda = await cg.process_lambda(
                retry_conf, [(_PDU_SPAN, "request")], return_type=cg.bool_
            )
            cg.add(var.set_retry(retry_lambda))
    if not_sent_conf := config.get(CONF_ON_NOT_SENT):
        await automation.build_automation(
            var.get_not_sent_trigger(), [(_PDU_SPAN, "request")], not_sent_conf
        )
    return var


@automation.register_action(
    "modbus_client.send",
    ModbusClientSendAction,
    MODBUS_CLIENT_SEND_SCHEMA,
    synchronous=True,
)
async def modbus_client_send_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    template_ = await cg.templatable(config[CONF_PDU], args, _PDU_BUFFER)
    cg.add(var.set_pdu(template_))
    return await register_client_action(
        var,
        config,
        args,
        [(_PDU_SPAN, "request"), (_PDU_SPAN, "response")],
    )


# --- Typed actions: request PDUs come from the device base's typed senders, replies from its dispatch,
# --- so on_response delivers decoded arguments (host-order words) instead of raw PDU spans.

_REGISTER_SPAN = cg.std_span.template(cg.uint16.operator("const"))

# The reply-handler pair every typed-dispatch action reports through. Kept in one place so the
# read/write-multiple schema (which cannot require start_address) shares it instead of drifting.
# Both use _handler_schema(): the decoded arguments (values span, bits view) point at buffers the hub
# reuses once the handler returns, so a deferring action would resume on freed memory. A reply the
# dispatch gate diverts (not a standard-conformant transaction) arrives at on_custom_response with the
# raw request/response PDUs; real device exceptions still arrive via on_error.
_REPLY_HANDLERS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ON_RESPONSE): _handler_schema(),
        cv.Optional(CONF_ON_CUSTOM_RESPONSE): _handler_schema(),
    }
)

# Every typed action addresses a register or coil range and reports through the shared reply handlers.
_TYPED_ACTION_SCHEMA = _ACTION_BASE_SCHEMA.extend(_REPLY_HANDLERS_SCHEMA).extend(
    {
        cv.Required(CONF_START_ADDRESS): cv.templatable(cv.hex_uint16_t),
    }
)


def _no_address_overflow(
    count_key: str, address_key: str = CONF_START_ADDRESS
) -> Callable[[ConfigType], ConfigType]:
    """Reject a range that runs past the 16-bit address space, which the device could never answer.

    Only literal configurations can be checked: either operand may be a lambda, and its value is not known
    until play(). The PDU builders repeat this check at runtime, so the lambda case is still rejected and
    logged - just later.
    """

    def validate(config: ConfigType) -> ConfigType:
        start = config[address_key]
        count = config[count_key]
        if isinstance(start, Lambda) or isinstance(count, Lambda):
            return config
        # A count key holds a number; a values key holds the list whose length is the count.
        length = count if isinstance(count, int) else len(count)
        if start + length > 0x10000:
            raise cv.Invalid(
                f"{address_key} 0x{start:04X} plus {length} entities runs past the end of the "
                f"16-bit address space (last addressable entity is 0xFFFF)",
                path=[address_key],
            )
        return config

    return validate


def _read_schema(max_count: int) -> cv.All:
    """Read action schema. The spec sets the read ceiling per function code, so each one passes its own."""
    return cv.All(
        _TYPED_ACTION_SCHEMA.extend(
            {
                cv.Optional(CONF_COUNT, default=1): cv.templatable(
                    cv.int_range(min=1, max=max_count)
                ),
            }
        ),
        _no_address_overflow(CONF_COUNT),
    )


def _write_multiple_schema(item: Callable[[Any], Any], max_values: int) -> cv.All:
    """Multi-write action schema, differing only in the element type and the spec's per-function limit."""
    return cv.All(
        _TYPED_ACTION_SCHEMA.extend(
            {
                cv.Required(CONF_VALUES): cv.templatable(
                    cv.All(cv.ensure_list(item), cv.Length(min=1, max=max_values))
                ),
            }
        ),
        _no_address_overflow(CONF_VALUES),
    )


_READ_REGISTERS_SCHEMA = _read_schema(modbus.MAX_NUM_OF_REGISTERS_TO_READ)

_WRITE_SINGLE_REGISTER_SCHEMA = _TYPED_ACTION_SCHEMA.extend(
    {cv.Required(CONF_VALUE): cv.templatable(cv.hex_uint16_t)}
)

# A coil is one bit, so the value is a boolean - the wire only carries 0x0000 or 0xFF00.
_WRITE_SINGLE_COIL_SCHEMA = _TYPED_ACTION_SCHEMA.extend(
    {cv.Required(CONF_VALUE): cv.templatable(cv.boolean)}
)


async def _read_registers_to_code(config, action_id, template_arg, args, holding):
    var = cg.new_Pvariable(action_id, template_arg, holding)
    cg.add(var.set_count(await cg.templatable(config[CONF_COUNT], args, cg.uint16)))
    return await register_client_action(var, config, args, [(_REGISTER_SPAN, "values")])


@automation.register_action(
    "modbus_client.read_holding_registers",
    ReadRegistersAction,
    _READ_REGISTERS_SCHEMA,
    synchronous=True,
)
async def read_holding_registers_to_code(config, action_id, template_arg, args):
    return await _read_registers_to_code(config, action_id, template_arg, args, True)


@automation.register_action(
    "modbus_client.read_input_registers",
    ReadRegistersAction,
    _READ_REGISTERS_SCHEMA,
    synchronous=True,
)
async def read_input_registers_to_code(config, action_id, template_arg, args):
    return await _read_registers_to_code(config, action_id, template_arg, args, False)


async def _write_single_to_code(config, action_id, template_arg, args, value_type):
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(var.set_value(await cg.templatable(config[CONF_VALUE], args, value_type)))
    return await register_client_action(var, config, args, [])


@automation.register_action(
    "modbus_client.write_single_register",
    WriteSingleRegisterAction,
    _WRITE_SINGLE_REGISTER_SCHEMA,
    synchronous=True,
)
async def write_single_register_to_code(config, action_id, template_arg, args):
    return await _write_single_to_code(config, action_id, template_arg, args, cg.uint16)


@automation.register_action(
    "modbus_client.write_single_coil",
    WriteSingleCoilAction,
    _WRITE_SINGLE_COIL_SCHEMA,
    synchronous=True,
)
async def write_single_coil_to_code(config, action_id, template_arg, args):
    return await _write_single_to_code(config, action_id, template_arg, args, cg.bool_)


_READ_COILS_SCHEMA = _read_schema(modbus.MAX_NUM_OF_COILS_TO_READ)
_READ_DISCRETE_INPUTS_SCHEMA = _read_schema(modbus.MAX_NUM_OF_DISCRETE_INPUTS_TO_READ)


async def _read_bits_to_code(config, action_id, template_arg, args, coils):
    var = cg.new_Pvariable(action_id, template_arg, coils)
    cg.add(var.set_count(await cg.templatable(config[CONF_COUNT], args, cg.uint16)))
    return await register_client_action(var, config, args, [(PackedBits, "bits")])


@automation.register_action(
    "modbus_client.read_coils",
    ReadBitsAction,
    _READ_COILS_SCHEMA,
    synchronous=True,
)
async def read_coils_to_code(config, action_id, template_arg, args):
    return await _read_bits_to_code(config, action_id, template_arg, args, True)


@automation.register_action(
    "modbus_client.read_discrete_inputs",
    ReadBitsAction,
    _READ_DISCRETE_INPUTS_SCHEMA,
    synchronous=True,
)
async def read_discrete_inputs_to_code(config, action_id, template_arg, args):
    return await _read_bits_to_code(config, action_id, template_arg, args, False)


_WRITE_MULTIPLE_REGISTERS_SCHEMA = _write_multiple_schema(
    cv.hex_uint16_t, modbus.MAX_NUM_OF_REGISTERS_TO_WRITE
)

_WRITE_MULTIPLE_COILS_SCHEMA = _write_multiple_schema(
    cv.boolean, modbus.MAX_NUM_OF_COILS_TO_WRITE
)


@automation.register_action(
    "modbus_client.write_multiple_registers",
    WriteMultipleRegistersAction,
    _WRITE_MULTIPLE_REGISTERS_SCHEMA,
    synchronous=True,
)
async def write_multiple_registers_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    values = config[CONF_VALUES]
    if cg.is_template(values):
        templ = await cg.templatable(values, args, cg.std_vector.template(cg.uint16))
        cg.add(var.set_values_template(templ))
    else:
        # A static list goes to flash, so play() sends straight from there without allocating.
        arr_id = ID(f"{action_id}_values", is_declaration=True, type=cg.uint16)
        arr = cg.static_const_array(arr_id, cg.ArrayInitializer(*values))
        cg.add(var.set_values_static(arr, len(values)))
    return await register_client_action(var, config, args, [])


@automation.register_action(
    "modbus_client.write_multiple_coils",
    WriteMultipleCoilsAction,
    _WRITE_MULTIPLE_COILS_SCHEMA,
    synchronous=True,
)
async def write_multiple_coils_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    values = config[CONF_VALUES]
    if cg.is_template(values):
        templ = await cg.templatable(values, args, cg.std_vector.template(cg.bool_))
        cg.add(var.set_values_template(templ))
    else:
        # Pack to wire layout (LSB first) here, so the runtime neither allocates nor packs.
        packed = bytearray(_packed_bit_bytes(len(values)))
        for i, coil in enumerate(values):
            if coil:
                packed[i // 8] |= 1 << (i % 8)
        arr_id = ID(f"{action_id}_values", is_declaration=True, type=cg.uint8)
        arr = cg.static_const_array(arr_id, cg.ArrayInitializer(*packed))
        cg.add(var.set_values_static(arr, len(values)))
    return await register_client_action(var, config, args, [])


# Read/write multiple registers (FC 0x17) writes one register block and reads another in a single
# transaction, so it has two address ranges and uses read_address/write_address instead of start_address.
# Note the two meanings of `values`: here it is the block being WRITTEN, while in on_response the lambda
# argument `values` is the block that was READ BACK (host-order words, the same shape as
# read_holding_registers, so a caller can feed it through the same handler).
_READ_WRITE_MULTIPLE_REGISTERS_SCHEMA = cv.All(
    _ACTION_BASE_SCHEMA.extend(_REPLY_HANDLERS_SCHEMA).extend(
        {
            cv.Required(CONF_READ_ADDRESS): cv.templatable(cv.hex_uint16_t),
            cv.Optional(CONF_READ_COUNT, default=1): cv.templatable(
                cv.int_range(min=1, max=modbus.MAX_NUM_OF_REGISTERS_TO_READ)
            ),
            cv.Required(CONF_WRITE_ADDRESS): cv.templatable(cv.hex_uint16_t),
            cv.Required(CONF_VALUES): cv.templatable(
                cv.All(
                    cv.ensure_list(cv.hex_uint16_t),
                    cv.Length(min=1, max=modbus.MAX_NUM_OF_REGISTERS_TO_WRITE_RW),
                )
            ),
        }
    ),
    _no_address_overflow(CONF_READ_COUNT, CONF_READ_ADDRESS),
    _no_address_overflow(CONF_VALUES, CONF_WRITE_ADDRESS),
)


@automation.register_action(
    "modbus_client.read_write_multiple_registers",
    ReadWriteMultipleRegistersAction,
    _READ_WRITE_MULTIPLE_REGISTERS_SCHEMA,
    synchronous=True,
)
async def read_write_multiple_registers_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(
        var.set_read_address(
            await cg.templatable(config[CONF_READ_ADDRESS], args, cg.uint16)
        )
    )
    cg.add(
        var.set_read_count(
            await cg.templatable(config[CONF_READ_COUNT], args, cg.uint16)
        )
    )
    cg.add(
        var.set_write_address(
            await cg.templatable(config[CONF_WRITE_ADDRESS], args, cg.uint16)
        )
    )
    values = config[CONF_VALUES]
    if cg.is_template(values):
        templ = await cg.templatable(values, args, cg.std_vector.template(cg.uint16))
        cg.add(var.set_values_template(templ))
    else:
        # A static list goes to flash, so play() sends straight from there without allocating.
        arr_id = ID(f"{action_id}_values", is_declaration=True, type=cg.uint16)
        arr = cg.static_const_array(arr_id, cg.ArrayInitializer(*values))
        cg.add(var.set_values_static(arr, len(values)))
    return await register_client_action(var, config, args, [(_REGISTER_SPAN, "values")])
