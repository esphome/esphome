import esphome.codegen as cg

modbus_ns = cg.esphome_ns.namespace("modbus")
modbus_helpers_ns = modbus_ns.namespace("helpers")

ModbusFunctionCode_ns = modbus_ns.namespace("ModbusFunctionCode")
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

ModbusRegisterType_ns = modbus_ns.namespace("ModbusRegisterType")
ModbusRegisterType = ModbusRegisterType_ns.enum("ModbusRegisterType")

MODBUS_WRITE_REGISTER_TYPE = {
    "custom": ModbusRegisterType.CUSTOM,
    "coil": ModbusRegisterType.COIL,
    "holding": ModbusRegisterType.HOLDING,
}

MODBUS_REGISTER_TYPE = {
    **MODBUS_WRITE_REGISTER_TYPE,
    "discrete_input": ModbusRegisterType.DISCRETE_INPUT,
    "read": ModbusRegisterType.READ,
}

SensorValueType_ns = modbus_helpers_ns.namespace("SensorValueType")
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
