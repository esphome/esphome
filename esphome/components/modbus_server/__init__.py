import esphome.codegen as cg
from esphome.components import modbus
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID
from esphome.cpp_helpers import logging

from .const import (
    CONF_ENABLED,
    CONF_REGISTER_LAST_ADDRESS,
    CONF_REGISTER_VALUE,
    CONF_SERVER_COURTESY_RESPONSE,
    CONF_VALUE_TYPE,
)

CODEOWNERS = ["@martgras"]

AUTO_LOAD = ["modbus"]

CONF_READ_LAMBDA = "read_lambda"
CONF_WRITE_LAMBDA = "write_lambda"
CONF_SERVER_REGISTERS = "server_registers"
MULTI_CONF = True

modbus_server_ns = cg.esphome_ns.namespace("modbus_server")
modbus_ns = cg.esphome_ns.namespace("modbus")
ModbusRole = modbus_ns.enum("ModbusRole")
ModbusServer = modbus_server_ns.class_(
    "ModbusServer", cg.PollingComponent, modbus.ModbusDevice
)

ServerCourtesyResponse = modbus_server_ns.struct("ServerCourtesyResponse")
ServerRegister = modbus_server_ns.struct("ServerRegister")

ModbusFunctionCode_ns = modbus_ns.namespace("FunctionCode")
ModbusFunctionCode = ModbusFunctionCode_ns.enum("FunctionCode")
MODBUS_FUNCTION_CODE = {
    "read_coils": ModbusFunctionCode.READ_COILS,
    "read_discrete_inputs": ModbusFunctionCode.READ_DISCRETE_INPUTS,
    "read_holding_registers": ModbusFunctionCode.READ_HOLDING_REGISTERS,
    "read_input_registers": ModbusFunctionCode.READ_INPUT_REGISTERS,
    "write_single_coil": ModbusFunctionCode.WRITE_SINGLE_COIL,
    "write_single_register": ModbusFunctionCode.WRITE_SINGLE_REGISTER,
    "write_multiple_coils": ModbusFunctionCode.WRITE_MULTIPLE_COILS,
    "write_multiple_registers": ModbusFunctionCode.WRITE_MULTIPLE_REGISTERS,
}

SensorValueType_ns = modbus_server_ns.namespace("SensorValueType")
SensorValueType = SensorValueType_ns.enum("SensorValueType")
SENSOR_VALUE_TYPE = {
    "RAW": SensorValueType.RAW,
    "U_WORD": SensorValueType.U_WORD,
    "S_WORD": SensorValueType.S_WORD,
    "U_DWORD": SensorValueType.U_DWORD,
    "U_DWORD_R": SensorValueType.U_DWORD_R,
    "S_DWORD": SensorValueType.S_DWORD,
    "S_DWORD_R": SensorValueType.S_DWORD_R,
    "U_QWORD": SensorValueType.U_QWORD,
    "U_QWORD_R": SensorValueType.U_QWORD_R,
    "S_QWORD": SensorValueType.S_QWORD,
    "S_QWORD_R": SensorValueType.S_QWORD_R,
    "FP32": SensorValueType.FP32,
    "FP32_R": SensorValueType.FP32_R,
}

TYPE_REGISTER_MAP = {
    "RAW": 1,
    "U_WORD": 1,
    "S_WORD": 1,
    "U_DWORD": 2,
    "U_DWORD_R": 2,
    "S_DWORD": 2,
    "S_DWORD_R": 2,
    "U_QWORD": 4,
    "U_QWORD_R": 4,
    "S_QWORD": 4,
    "S_QWORD_R": 4,
    "FP32": 2,
    "FP32_R": 2,
}

CPP_TYPE_REGISTER_MAP = {
    "RAW": cg.uint16,
    "U_WORD": cg.uint16,
    "S_WORD": cg.int16,
    "U_DWORD": cg.uint32,
    "U_DWORD_R": cg.uint32,
    "S_DWORD": cg.int32,
    "S_DWORD_R": cg.int32,
    "U_QWORD": cg.uint64,
    "U_QWORD_R": cg.uint64,
    "S_QWORD": cg.int64,
    "S_QWORD_R": cg.int64,
    "FP32": cg.float_,
    "FP32_R": cg.float_,
}

_LOGGER = logging.getLogger(__name__)

SERVER_COURTESY_RESPONSE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLED, default=False): cv.boolean,
        cv.Optional(CONF_REGISTER_LAST_ADDRESS, default=0xFFFF): cv.hex_uint16_t,
        cv.Optional(CONF_REGISTER_VALUE, default=0): cv.hex_uint16_t,
    }
)

ModbusServerRegisterSchema = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ServerRegister),
        cv.Required(CONF_ADDRESS): cv.positive_int,
        cv.Optional(CONF_VALUE_TYPE, default="U_WORD"): cv.enum(SENSOR_VALUE_TYPE),
        cv.Required(CONF_READ_LAMBDA): cv.returning_lambda,
        cv.Optional(CONF_WRITE_LAMBDA): cv.returning_lambda,
    }
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModbusServer),
            cv.Optional(CONF_SERVER_COURTESY_RESPONSE): SERVER_COURTESY_RESPONSE_SCHEMA,
            cv.Optional(
                CONF_SERVER_REGISTERS,
            ): cv.ensure_list(ModbusServerRegisterSchema),
        }
    ).extend(modbus.modbus_server_device_schema(0x01)),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if server_courtesy_response := config.get(CONF_SERVER_COURTESY_RESPONSE):
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
    if CONF_SERVER_REGISTERS in config:
        for server_register in config[CONF_SERVER_REGISTERS]:
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
                        return_type=cpp_type,
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
            cg.add(var.add_server_register(server_register_var))
    await register_modbus_device(var, config)


async def register_modbus_device(var, config):
    cg.add(var.set_address(config[CONF_ADDRESS]))
    await cg.register_component(var, config)
    return await modbus.register_modbus_device(var, config)
