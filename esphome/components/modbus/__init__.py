from __future__ import annotations

import logging
from typing import Any, Literal

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
from esphome.cpp_helpers import gpio_pin_expression
import esphome.final_validate as fv
from esphome.types import ConfigType

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


def command_options_schema(
    *, direction: Literal["read", "write"], templatable: bool = False
) -> dict:
    """Schema fragment for the per-command options a component forwards to the hub
    (modbus::CommandOptions). Extend this into any schema that queues commands. Keys are
    direction-specific so a schema never offers an option the hub would strip (e.g.
    continuous on a write); the write side has no options yet.

    With templatable=False the values are static: build the C++ initializer with
    command_options_expression() using the same direction. With templatable=True the keys
    also accept lambdas (for actions, where trigger arguments are in scope): the consumer's
    C++ class declares a TEMPLATABLE_VALUE per option and assembles CommandOptions at play
    time; register the values with register_templatable_command_options().
    """
    options = {}
    if direction == "read":
        validator = cv.templatable(cv.boolean) if templatable else cv.boolean
        options[cv.Optional(CONF_CONTINUOUS, default=False)] = validator
    return options


def command_options_expression(
    config: ConfigType, *, direction: Literal["read", "write"]
) -> cg.StructInitializer:
    """Build the modbus::CommandOptions initializer for a config validated with a static
    (templatable=False) command_options_schema() of the same direction."""
    fields = []
    if direction == "read":
        fields.append(("continuous", config[CONF_CONTINUOUS]))
    return cg.StructInitializer(CommandOptions, *fields)


async def register_templatable_command_options(
    var, config: ConfigType, args: list, *, direction: Literal["read", "write"]
) -> None:
    """Generate the set_<option>() calls for a config validated with a templatable
    command_options_schema() of the same direction. The consumer's C++ class declares a
    matching TEMPLATABLE_VALUE per option (e.g. TEMPLATABLE_VALUE(bool, continuous)) and
    builds the CommandOptions it sends by evaluating them with the trigger arguments.
    """
    if direction == "read":
        template_ = await cg.templatable(config[CONF_CONTINUOUS], args, bool)
        cg.add(var.set_continuous(template_))


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


async def to_code(config):
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


def _validate_server_address(value: Any) -> int:
    address = cv.hex_uint8_t(value)
    # The broadcast address (0) is delivered to every device and is never answered (Modbus 4.1),
    # so it cannot identify an individual server device.
    if address == 0:
        raise cv.Invalid(
            "Address 0 is the Modbus broadcast address and cannot be used as a "
            "server device address. Assign a unique unit address instead."
        )
    return address


def modbus_device_schema(default_address, role: Literal["client", "server"] = "client"):
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
):
    def validate_role(value):
        assert role in MODBUS_ROLES
        if value != role:
            raise cv.Invalid(f"Component {name} requires role to be {role}")
        return value

    def validate_hub(hub_config):
        hub_schema = {}
        if role is not None:
            hub_schema[cv.Required(CONF_ROLE)] = validate_role

        return cv.Schema(hub_schema, extra=cv.ALLOW_EXTRA)(hub_config)

    return cv.Schema(
        {cv.Required(CONF_MODBUS_ID): fv.id_declaration_match_schema(validate_hub)},
        extra=cv.ALLOW_EXTRA,
    )


async def register_modbus_client_device(var, config):
    parent = await cg.get_variable(config[CONF_MODBUS_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_address(config[CONF_ADDRESS]))


async def register_modbus_server_device(var, config):
    parent = await cg.get_variable(config[CONF_MODBUS_ID])
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(parent.register_device(var))


async def register_modbus_device(var, config):
    # Remove before 2026.12.0
    _LOGGER.warning(
        "'register_modbus_device' is deprecated, use 'register_modbus_client_device' "
        "instead. Will be removed in 2026.12.0"
    )
    return await register_modbus_client_device(var, config)
