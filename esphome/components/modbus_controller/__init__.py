import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import modbus
from esphome.const import CONF_ID, CONF_ADDRESS
from esphome.cpp_helpers import logging
from .const import (
    CONF_COMMAND_THROTTLE,
)

CODEOWNERS = ["@martgras"]

AUTO_LOAD = ["modbus"]

MULTI_CONF = True

# pylint: disable=invalid-name
modbus_controller_ns = cg.esphome_ns.namespace("modbus_controller")
ModbusController = modbus_controller_ns.class_(
    "ModbusController", cg.PollingComponent, modbus.ModbusDevice
)

SensorItem = modbus_controller_ns.struct("SensorItem")

ModbusFunctionCode_ns = modbus_controller_ns.namespace("ModbusFunctionCode")
ModbusFunctionCode = ModbusFunctionCode_ns.enum("ModbusFunctionCode")
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

ModbusRegisterType_ns = modbus_controller_ns.namespace("ModbusRegisterType")
ModbusRegisterType = ModbusRegisterType_ns.enum("ModbusRegisterType")
MODBUS_REGISTER_TYPE = {
    "coil": ModbusRegisterType.COIL,
    "discrete_input": ModbusRegisterType.DISCRETE_INPUT,
    "holding": ModbusRegisterType.HOLDING,
    "read": ModbusRegisterType.READ,
}

SensorValueType_ns = modbus_controller_ns.namespace("SensorValueType")
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
    "U_QWORDU_R": SensorValueType.U_QWORD_R,
    "S_QWORD": SensorValueType.S_QWORD,
    "U_QWORD_R": SensorValueType.S_QWORD_R,
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
    "U_QWORDU_R": 4,
    "S_QWORD": 4,
    "U_QWORD_R": 4,
    "FP32": 2,
    "FP32_R": 2,
}

MULTI_CONF = True

_LOGGER = logging.getLogger(__name__)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModbusController),
            cv.Optional(
                CONF_COMMAND_THROTTLE, default="0ms"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(modbus.modbus_device_schema(0x01))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_COMMAND_THROTTLE])
    cg.add(var.set_command_throttle(config[CONF_COMMAND_THROTTLE]))
    await register_modbus_device(var, config)


async def register_modbus_device(var, config):
    cg.add(var.set_address(config[CONF_ADDRESS]))
    await cg.register_component(var, config)
    return await modbus.register_modbus_device(var, config)


def function_code_to_register(function_code):
    FUNCTION_CODE_TYPE_MAP = {
        "read_coils": ModbusRegisterType.COIL,
        "read_discrete_inputs": ModbusRegisterType.DISCRETE,
        "read_holding_registers": ModbusRegisterType.HOLDING,
        "read_input_registers": ModbusRegisterType.READ,
        "write_single_coil": ModbusRegisterType.COIL,
        "write_single_register": ModbusRegisterType.HOLDING,
        "write_multiple_coils": ModbusRegisterType.COIL,
        "write_multiple_registers": ModbusRegisterType.HOLDING,
    }
    return FUNCTION_CODE_TYPE_MAP[function_code]


def find_by_value(dict, find_value):
    for (key, value) in MODBUS_REGISTER_TYPE.items():
        print(find_value, value)
        if find_value == value:
            return key
    return "not found"
