import binascii
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import modbus
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_NAME, CONF_LAMBDA, CONF_OFFSET
from esphome.cpp_helpers import logging
from .const import (
    CONF_BITMASK,
    CONF_BYTE_OFFSET,
    CONF_COMMAND_THROTTLE,
    CONF_CUSTOM_COMMAND,
    CONF_FORCE_NEW_RANGE,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_REGISTER_COUNT,
    CONF_REGISTER_TYPE,
    CONF_RESPONSE_SIZE,
    CONF_SKIP_UPDATES,
    CONF_VALUE_TYPE,
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


ModbusItemBaseSchema = cv.Schema(
    {
        cv.GenerateID(CONF_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
        cv.Optional(CONF_ADDRESS): cv.positive_int,
        cv.Optional(CONF_CUSTOM_COMMAND): cv.ensure_list(cv.hex_uint8_t),
        cv.Exclusive(
            CONF_OFFSET,
            "offset",
            f"{CONF_OFFSET} and {CONF_BYTE_OFFSET} can't be used together",
        ): cv.positive_int,
        cv.Exclusive(
            CONF_BYTE_OFFSET,
            "offset",
            f"{CONF_OFFSET} and {CONF_BYTE_OFFSET} can't be used together",
        ): cv.positive_int,
        cv.Optional(CONF_BITMASK, default=0xFFFFFFFF): cv.hex_uint32_t,
        cv.Optional(CONF_SKIP_UPDATES, default=0): cv.positive_int,
        cv.Optional(CONF_FORCE_NEW_RANGE, default=False): cv.boolean,
        cv.Optional(CONF_LAMBDA): cv.returning_lambda,
        cv.Optional(CONF_RESPONSE_SIZE, default=0): cv.positive_int,
    },
)


def validate_modbus_register(config):
    if CONF_CUSTOM_COMMAND not in config and CONF_ADDRESS not in config:
        raise cv.Invalid(
            f" {CONF_ADDRESS} is a required property if '{CONF_CUSTOM_COMMAND}:' isn't used"
        )
    if CONF_CUSTOM_COMMAND in config and CONF_REGISTER_TYPE in config:
        raise cv.Invalid(
            f"can't use '{CONF_REGISTER_TYPE}:' together with '{CONF_CUSTOM_COMMAND}:'",
        )

    if CONF_CUSTOM_COMMAND not in config and CONF_REGISTER_TYPE not in config:
        raise cv.Invalid(
            f" {CONF_REGISTER_TYPE} is a required property if '{CONF_CUSTOM_COMMAND}:' isn't used"
        )
    return config


def modbus_calc_properties(config):
    byte_offset = 0
    reg_count = 0
    if CONF_OFFSET in config:
        byte_offset = config[CONF_OFFSET]
    # A CONF_BYTE_OFFSET setting overrides CONF_OFFSET
    if CONF_BYTE_OFFSET in config:
        byte_offset = config[CONF_BYTE_OFFSET]
    if CONF_REGISTER_COUNT in config:
        reg_count = config[CONF_REGISTER_COUNT]
    if CONF_VALUE_TYPE in config:
        value_type = config[CONF_VALUE_TYPE]
        if reg_count == 0:
            reg_count = TYPE_REGISTER_MAP[value_type]
    if CONF_CUSTOM_COMMAND in config:
        if CONF_ADDRESS not in config:
            # generate a unique modbus address using the hash of the name
            # CONF_NAME set even if only CONF_ID is used.
            # a modbus register address is required to add the item to sensormap
            value = config[CONF_NAME]
            if isinstance(value, str):
                value = value.encode()
            config[CONF_ADDRESS] = binascii.crc_hqx(value, 0)
        config[CONF_REGISTER_TYPE] = ModbusRegisterType.CUSTOM
        config[CONF_FORCE_NEW_RANGE] = True
    return byte_offset, reg_count


async def add_modbus_base_properties(
    var, config, sensor_type, lamdba_param_type=cg.float_, lamdba_return_type=float
):
    if CONF_CUSTOM_COMMAND in config:
        cg.add(var.set_custom_data(config[CONF_CUSTOM_COMMAND]))

    if config[CONF_RESPONSE_SIZE] > 0:
        cg.add(var.set_register_size(config[CONF_RESPONSE_SIZE]))

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [
                (sensor_type.operator("ptr"), "item"),
                (lamdba_param_type, "x"),
                (
                    cg.std_vector.template(cg.uint8).operator("const").operator("ref"),
                    "data",
                ),
            ],
            return_type=cg.optional.template(lamdba_return_type),
        )
        cg.add(var.set_template(template_))


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
