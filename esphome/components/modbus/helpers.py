import esphome.codegen as cg

modbus_ns = cg.esphome_ns.namespace("modbus")
modbus_helpers_ns = modbus_ns.namespace("helpers")

RegisterValues = modbus_ns.class_("RegisterValues")
PduBuffer = modbus_helpers_ns.class_("PduBuffer")

FunctionCode_ns = modbus_ns.namespace("FunctionCode")
FunctionCode = FunctionCode_ns.enum("FunctionCode")

MODBUS_FUNCTION_CODE = FunctionCode.options(
    "read_coils",
    "read_discrete_inputs",
    "read_holding_registers",
    "read_input_registers",
    "write_single_coil",
    "write_single_register",
    "write_multiple_coils",
    "write_multiple_registers",
)

EntityType_ns = modbus_ns.namespace("EntityType")
EntityType = EntityType_ns.enum("EntityType")

MODBUS_WRITE_REGISTER_TYPE = EntityType.options("custom", "coil", "holding")

MODBUS_REGISTER_TYPE = {
    **MODBUS_WRITE_REGISTER_TYPE,
    "discrete_input": EntityType.DISCRETE_INPUT,
    "read": EntityType.INPUT_REGISTER,
    "input": EntityType.INPUT_REGISTER,
}

SensorValueType_ns = modbus_helpers_ns.namespace("SensorValueType")
SensorValueType = SensorValueType_ns.enum("SensorValueType")
SENSOR_VALUE_TYPE = SensorValueType.options(
    "RAW",
    "U_WORD",
    "U_WORD_S",
    "S_WORD",
    "S_WORD_S",
    "U_DWORD",
    "U_DWORD_R",
    "S_DWORD",
    "S_DWORD_R",
    "U_QWORD",
    "U_QWORD_R",
    "S_QWORD",
    "S_QWORD_R",
    "FP32",
    "FP32_R",
)

TYPE_REGISTER_MAP = {
    "RAW": 1,
    "U_WORD": 1,
    "U_WORD_S": 1,
    "S_WORD": 1,
    "S_WORD_S": 1,
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
    "U_WORD_S": cg.uint16,
    "S_WORD": cg.int16,
    "S_WORD_S": cg.int16,
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
