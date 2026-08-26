from __future__ import annotations

import logging
from typing import Any, Literal, NamedTuple

from esphome import pins
import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_CONTINUOUS,
    CONF_DISABLE_CRC,
    CONF_FLOW_CONTROL_PIN,
    CONF_ID,
)
from esphome.cpp_generator import MockObj
from esphome.cpp_helpers import gpio_pin_expression
import esphome.final_validate as fv
from esphome.types import ConfigType, TemplateArgsType

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["uart"]
# Loading the hub makes the modbus_client.* actions available (they are registry entries only; no code is
# generated unless a config uses one).
AUTO_LOAD = ["modbus_client"]

# Mirrors modbus::MAX_PDU_SIZE in modbus_definitions.h: 256-byte RTU frame minus address and CRC.
MAX_PDU_SIZE = 253

# Mirror the per-function entity count limits from modbus_definitions.h. Keep these in step with the
# C++ constants of the same name; the spec sets a different ceiling for each function code.
MAX_NUM_OF_COILS_TO_READ = 2000
MAX_NUM_OF_DISCRETE_INPUTS_TO_READ = 2000
MAX_NUM_OF_COILS_TO_WRITE = 1968
MAX_NUM_OF_REGISTERS_TO_READ = 125
MAX_NUM_OF_REGISTERS_TO_WRITE = 123
MAX_NUM_OF_REGISTERS_TO_WRITE_RW = 121

modbus_ns = cg.esphome_ns.namespace("modbus")
Modbus = modbus_ns.class_("Modbus", cg.Component, uart.UARTDevice)
ModbusServer = modbus_ns.class_("ModbusServerHub", Modbus)
ModbusClient = modbus_ns.class_("ModbusClientHub", Modbus)
ModbusDevice = modbus_ns.class_("ModbusDevice")
ModbusClientDevice = modbus_ns.class_("ModbusClientDevice")
ModbusServerDevice = modbus_ns.class_("ModbusServerDevice")
CommandOptions = modbus_ns.struct("CommandOptions")
MULTI_CONF = True

CONF_ROLE = "role"
CONF_MODBUS_ID = "modbus_id"
CONF_SEND_WAIT_TIME = "send_wait_time"
CONF_TURNAROUND_TIME = "turnaround_time"

MODBUS_ROLES = ["client", "server"]


class _CommandOption(NamedTuple):
    """One per-command option forwarded to the hub (modbus::CommandOptions)."""

    conf_key: str
    field: str  # the C++ field, and so the set_<field>() setter name
    validator: Any  # the static (non-templatable) validator for the key
    cpp_type: Any  # the C++ type the value is generated as
    default: Any


# Per-direction command options. Single-sourcing the schema and the setter generation here keeps
# them from drifting; the C++ side must add the matching field per the rules documented on
# CommandOptions (modbus.h).
_COMMAND_OPTIONS: dict[str, list[_CommandOption]] = {
    "read": [_CommandOption(CONF_CONTINUOUS, "continuous", cv.boolean, bool, False)],
    "write": [],
}


def _command_options(direction: str) -> list[_CommandOption]:
    try:
        return _COMMAND_OPTIONS[direction]
    except KeyError:
        raise ValueError(f"unknown command-options direction {direction!r}") from None


# The write (mutating) function codes, matching modbus::helpers::is_function_code_write(). 0x17
# (read/write multiple) is included: it mutates, so the hub treats it as a write despite its read half.
_WRITE_FUNCTION_CODES = frozenset({0x05, 0x06, 0x0F, 0x10, 0x16, 0x17})


def is_function_code_write(function_code: int) -> bool:
    """True if the Modbus function code writes (mutates). The exception bit (0x80) is masked off first,
    so an exception-flagged code still classifies by its base code - stricter than the runtime hub,
    whose classify() treats an exception-flagged code as a read. Keep in sync with
    modbus::helpers::is_function_code_write()."""
    return function_code & 0x7F in _WRITE_FUNCTION_CODES


def command_options_schema(
    *, direction: Literal["read", "write"], templatable: bool = False
) -> dict[cv.Optional, Any]:
    """Schema fragment for the per-command options a component forwards to the hub
    (modbus::CommandOptions). Extend this into any schema that queues commands. Keys are
    direction-specific so a schema never offers an option the hub would strip (e.g.
    continuous on a write); the write side has no options yet. For actions (templatable=True the
    keys also accept lambdas), register the values with register_templatable_command_options().
    """
    return {
        cv.Optional(option.conf_key, default=option.default): (
            cv.templatable(option.validator) if templatable else option.validator
        )
        for option in _command_options(direction)
    }


def command_options_expression(
    config: ConfigType, *, direction: Literal["read", "write"]
) -> cg.StructInitializer:
    """Build the modbus::CommandOptions initializer for a config validated with
    command_options_schema() of the same direction. For static (non-templatable) options only;
    actions with lambda values use register_templatable_command_options() instead.
    """
    return cg.StructInitializer(
        CommandOptions,
        *(
            # Construct the value as its declared cpp_type, so a future non-bool option (enum,
            # uint16_t, ...) is emitted with the right type instead of whatever safe_exp() infers.
            (option.field, option.cpp_type(config[option.conf_key]))
            for option in _command_options(direction)
            if option.conf_key in config
        ),
    )


async def register_templatable_command_options(
    var: MockObj, config: ConfigType, args: TemplateArgsType, direction: str
) -> None:
    """Generate the set_<option>() calls for the given direction's command options present in config.
    Pass the same direction the action's command_options_schema() used, so the keys generated match
    the ones the schema offered - a write action never emits a read option's setter. Options the
    schema did not add are simply absent. The consumer's C++ class declares a matching
    TEMPLATABLE_VALUE per option (e.g. TEMPLATABLE_VALUE(bool, continuous)).
    """
    for option in _command_options(direction):
        if option.conf_key not in config:
            continue
        value = config[option.conf_key]
        # Skip codegen when the value is its C++ zero (TemplatableFn::value() returns T{} when
        # unset): behaviourally identical, and saves a thunk plus a setup() call per action.
        if cg.is_template(value) or value != type(value)():
            cg.add(
                getattr(var, f"set_{option.field}")(
                    await cg.templatable(value, args, option.cpp_type)
                )
            )


CONFIG_SCHEMA = cv.typed_schema(
    {
        "client": cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(ModbusClient),
                cv.Optional(CONF_FLOW_CONTROL_PIN): pins.gpio_output_pin_schema,
                cv.Optional(
                    CONF_SEND_WAIT_TIME, default="2000ms"
                ): cv.positive_time_period_milliseconds,
                cv.Optional(
                    CONF_TURNAROUND_TIME, default="600ms"
                ): cv.positive_time_period_milliseconds,
                # Remove before 2026.10.0
                cv.Optional(CONF_DISABLE_CRC): cv.invalid(
                    "'disable_crc' has been removed. The parser no longer requires it — remove this option."
                ),
            }
        )
        .extend(cv.COMPONENT_SCHEMA)
        .extend(uart.UART_DEVICE_SCHEMA),
        "server": cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(ModbusServer),
                cv.Optional(CONF_FLOW_CONTROL_PIN): pins.gpio_output_pin_schema,
                # Remove before 2026.10.0
                cv.Optional(CONF_DISABLE_CRC): cv.invalid(
                    "'disable_crc' has been removed. The parser no longer requires it — remove this option."
                ),
            }
        )
        .extend(cv.COMPONENT_SCHEMA)
        .extend(uart.UART_DEVICE_SCHEMA),
    },
    key=CONF_ROLE,
    default_type="client",
)


async def to_code(config: ConfigType) -> None:
    cg.add_global(modbus_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await uart.register_uart_device(var, config)

    if CONF_FLOW_CONTROL_PIN in config:
        pin = await gpio_pin_expression(config[CONF_FLOW_CONTROL_PIN])
        cg.add(var.set_flow_control_pin(pin))

    if config[CONF_ROLE] == "client":
        cg.add(var.set_send_wait_time(config[CONF_SEND_WAIT_TIME]))
        cg.add(var.set_turnaround_time(config[CONF_TURNAROUND_TIME]))


# The broadcast address (0) is delivered to every device and is never answered (Modbus 4.1),
# so it cannot identify an individual device or read anything back.
BROADCAST_ADDRESS = 0


def reject_broadcast_address(
    address: int, usage: str, guidance: str, path: list[str] | None = None
) -> None:
    """Raise cv.Invalid if `address` is the Modbus broadcast address (0).

    `usage` names how the address is being used (e.g. "a server device address") and `guidance`
    is a sentence telling the user what to do instead. Sharing the leading sentence here keeps the
    call sites (server device, modbus_controller) from drifting apart.
    """
    if address == BROADCAST_ADDRESS:
        raise cv.Invalid(
            f"Address 0 is the Modbus broadcast address and cannot be used as {usage}. {guidance}",
            path,
        )


def _validate_server_address(value: Any) -> int:
    address = cv.hex_uint8_t(value)
    reject_broadcast_address(
        address,
        "a server device address",
        "Assign a unique unit address instead.",
    )
    return address


def modbus_device_schema(
    default_address: int | None, role: Literal["client", "server"] = "client"
) -> cv.Schema:
    hub_type = ModbusClient if role == "client" else ModbusServer
    address_validator = _validate_server_address if role == "server" else cv.hex_uint8_t
    schema = {
        cv.GenerateID(CONF_MODBUS_ID): cv.use_id(hub_type),
    }
    if default_address is None:
        schema[cv.Required(CONF_ADDRESS)] = address_validator
    else:
        schema[cv.Optional(CONF_ADDRESS, default=default_address)] = address_validator
    return cv.Schema(schema)


def final_validate_modbus_device(
    name: str, *, role: Literal["server", "client"] | None = None
) -> cv.Schema:
    def validate_role(value: str) -> str:
        assert role in MODBUS_ROLES
        if value != role:
            raise cv.Invalid(f"Component {name} requires role to be {role}")
        return value

    def validate_hub(hub_config: ConfigType) -> ConfigType:
        hub_schema = {}
        if role is not None:
            hub_schema[cv.Required(CONF_ROLE)] = validate_role

        return cv.Schema(hub_schema, extra=cv.ALLOW_EXTRA)(hub_config)

    return cv.Schema(
        {cv.Required(CONF_MODBUS_ID): fv.id_declaration_match_schema(validate_hub)},
        extra=cv.ALLOW_EXTRA,
    )


async def register_modbus_client_device(var: MockObj, config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_MODBUS_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_address(config[CONF_ADDRESS]))


async def register_modbus_server_device(var: MockObj, config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_MODBUS_ID])
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(parent.register_device(var))


async def register_modbus_device(var: MockObj, config: ConfigType) -> None:
    # Remove before 2026.12.0
    _LOGGER.warning(
        "'register_modbus_device' is deprecated, use 'register_modbus_client_device' "
        "instead. Will be removed in 2026.12.0"
    )
    return await register_modbus_client_device(var, config)
