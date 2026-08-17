import esphome.codegen as cg
from esphome.components import modbus
from esphome.components.const import CONF_ENABLED
from esphome.components.modbus.helpers import (
    CPP_TYPE_REGISTER_MAP,
    SENSOR_VALUE_TYPE,
    TYPE_REGISTER_MAP,
)
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID
from esphome.types import ConfigType

from .const import (
    CONF_ALLOW_PARTIAL_READ,
    CONF_BITS,
    CONF_COURTESY_RESPONSE,
    CONF_READ_LAMBDA,
    CONF_REGISTER_LAST_ADDRESS,
    CONF_REGISTER_VALUE,
    CONF_REGISTERS,
    CONF_VALUE_TYPE,
    CONF_WRITE_LAMBDA,
)

CODEOWNERS = ["@exciton"]

AUTO_LOAD = ["modbus"]

MULTI_CONF = True

modbus_server_ns = cg.esphome_ns.namespace("modbus_server")
ModbusServer = modbus_server_ns.class_(
    "ModbusServer", cg.Component, modbus.ModbusServerDevice
)

ServerCourtesyResponse = modbus_server_ns.struct("ServerCourtesyResponse")
ServerRegister = modbus_server_ns.struct("ServerRegister")
ServerBit = modbus_server_ns.class_("ServerBit")

SERVER_COURTESY_RESPONSE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLED, default=False): cv.boolean,
        cv.Optional(CONF_REGISTER_LAST_ADDRESS, default=0xFFFF): cv.hex_uint16_t,
        cv.Optional(CONF_REGISTER_VALUE, default=0): cv.hex_uint16_t,
    }
)

# RAW has no numeric encoding, so it is not a valid server register type: a server value is produced by a
# lambda and encoded into registers, and on the server a RAW register would just be a single 16-bit word --
# use U_WORD for that. Restrict the choices to the encodable types.
SERVER_SENSOR_VALUE_TYPE = {
    key: value for key, value in SENSOR_VALUE_TYPE.items() if key != "RAW"
}

ModbusServerRegisterSchema = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ServerRegister),
        cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
        cv.Optional(CONF_VALUE_TYPE, default="U_WORD"): cv.enum(
            SERVER_SENSOR_VALUE_TYPE
        ),
        cv.Required(CONF_READ_LAMBDA): cv.returning_lambda,
        cv.Optional(CONF_WRITE_LAMBDA): cv.returning_lambda,
        cv.Optional(CONF_ALLOW_PARTIAL_READ, default=False): cv.boolean,
    }
)


ModbusServerBitSchema = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ServerBit),
        cv.Required(CONF_ADDRESS): cv.hex_uint16_t,
        cv.Required(CONF_READ_LAMBDA): cv.returning_lambda,
        cv.Optional(CONF_WRITE_LAMBDA): cv.returning_lambda,
    }
)


def _validate_unique_bit_addresses(config: ConfigType) -> ConfigType:
    # Coils and discrete inputs share one bit address space (like holding/input registers share the
    # register table), so each bit address may appear only once.
    seen: set[int] = set()
    for bit in config.get(CONF_BITS, []):
        address = bit[CONF_ADDRESS]
        if address in seen:
            raise cv.Invalid(
                f"Bit address 0x{address:04X} is configured more than once; coils and discrete "
                "inputs share one bit address space, so each address must be unique",
                path=[CONF_BITS],
            )
        seen.add(address)
    return config


def _validate_register_ranges(config: ConfigType) -> ConfigType:
    # Each register occupies [address, address + register_count); the whole span must fit inside the 16-bit
    # Modbus address space (0x0000-0xFFFF).
    for register in config.get(CONF_REGISTERS, []):
        address = register[CONF_ADDRESS]
        register_count = TYPE_REGISTER_MAP[register[CONF_VALUE_TYPE]]
        if address + register_count > 0x10000:
            raise cv.Invalid(
                f"Register at 0x{address:04X} spans {register_count} register(s) and runs past "
                "the end of the 16-bit address space (0xFFFF)",
                path=[CONF_REGISTERS],
            )
    return config


def _validate_no_overlapping_registers(config: ConfigType) -> ConfigType:
    # Each register occupies [address, address + register_count). Reject configs where any two ranges
    # overlap -- the same address twice, or a multi-register value straddling a neighbour -- since the
    # server resolves a request by the value containing an address and overlaps are ambiguous.
    spans = sorted(
        (register[CONF_ADDRESS], TYPE_REGISTER_MAP[register[CONF_VALUE_TYPE]])
        for register in config.get(CONF_REGISTERS, [])
    )
    for (address, register_count), (next_address, _) in zip(
        spans, spans[1:], strict=False
    ):
        if next_address < address + register_count:
            raise cv.Invalid(
                f"Register address 0x{next_address:04X} overlaps the register at 0x{address:04X}, "
                f"which spans {register_count} register(s); each register's address range must be unique",
                path=[CONF_REGISTERS],
            )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModbusServer),
            cv.Optional(CONF_COURTESY_RESPONSE): SERVER_COURTESY_RESPONSE_SCHEMA,
            cv.Optional(
                CONF_REGISTERS,
            ): cv.ensure_list(ModbusServerRegisterSchema),
            cv.Optional(CONF_BITS): cv.ensure_list(ModbusServerBitSchema),
        }
    ).extend(modbus.modbus_device_schema(0x01, role="server")),
    _validate_register_ranges,
    _validate_no_overlapping_registers,
    _validate_unique_bit_addresses,
)


def _final_validate(config: ConfigType) -> None:
    modbus.final_validate_modbus_device("modbus_server", role="server")(config)


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if server_courtesy_response := config.get(CONF_COURTESY_RESPONSE):
        cg.add(
            var.set_server_courtesy_response(
                cg.StructInitializer(
                    ServerCourtesyResponse,
                    ("enabled", server_courtesy_response[CONF_ENABLED]),
                    (
                        "register_last_address",
                        server_courtesy_response[CONF_REGISTER_LAST_ADDRESS],
                    ),
                    ("register_value", server_courtesy_response[CONF_REGISTER_VALUE]),
                )
            )
        )
    if CONF_REGISTERS in config:
        for server_register in config[CONF_REGISTERS]:
            server_register_var = cg.new_Pvariable(
                server_register[CONF_ID],
                server_register[CONF_ADDRESS],
                server_register[CONF_VALUE_TYPE],
                TYPE_REGISTER_MAP[server_register[CONF_VALUE_TYPE]],
            )
            cpp_type = CPP_TYPE_REGISTER_MAP[server_register[CONF_VALUE_TYPE]]
            cg.add(
                server_register_var.set_read_lambda(
                    cg.TemplateArguments(cpp_type),
                    await cg.process_lambda(
                        server_register[CONF_READ_LAMBDA],
                        [(cg.uint16, "address")],
                        return_type=cg.optional.template(cpp_type),
                    ),
                )
            )
            if CONF_WRITE_LAMBDA in server_register:
                cg.add(
                    server_register_var.set_write_lambda(
                        cg.TemplateArguments(cpp_type),
                        await cg.process_lambda(
                            server_register[CONF_WRITE_LAMBDA],
                            parameters=[(cg.uint16, "address"), (cpp_type, "x")],
                            return_type=cg.bool_,
                        ),
                    )
                )
            if server_register[CONF_ALLOW_PARTIAL_READ]:
                cg.add(server_register_var.set_allow_partial_read(True))
            cg.add(var.add_server_register(server_register_var))
    for server_bit in config.get(CONF_BITS, []):
        server_bit_var = cg.new_Pvariable(server_bit[CONF_ID], server_bit[CONF_ADDRESS])
        cg.add(
            server_bit_var.set_read_lambda(
                await cg.process_lambda(
                    server_bit[CONF_READ_LAMBDA],
                    [(cg.uint16, "address")],
                    return_type=cg.optional.template(cg.bool_),
                )
            )
        )
        if (write_lambda := server_bit.get(CONF_WRITE_LAMBDA)) is not None:
            cg.add(
                server_bit_var.set_write_lambda(
                    await cg.process_lambda(
                        write_lambda,
                        parameters=[(cg.uint16, "address"), (cg.bool_, "x")],
                        return_type=cg.bool_,
                    )
                )
            )
        cg.add(var.add_server_bit(server_bit_var))
    await cg.register_component(var, config)
    return await modbus.register_modbus_server_device(var, config)
