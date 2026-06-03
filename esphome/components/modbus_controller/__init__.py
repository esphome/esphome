import binascii

from esphome import automation
import esphome.codegen as cg
from esphome.components import modbus
from esphome.components.modbus.helpers import (
    MODBUS_REGISTER_TYPE,
    TYPE_REGISTER_MAP,
    ModbusRegisterType,
)
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_OFFSET
from esphome.cpp_helpers import logging

from .const import (
    CONF_ALLOW_DUPLICATE_COMMANDS,
    CONF_BITMASK,
    CONF_BYTE_OFFSET,
    CONF_COMMAND_THROTTLE,
    CONF_CUSTOM_COMMAND,
    CONF_FORCE_NEW_RANGE,
    CONF_MAX_CMD_RETRIES,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_OFFLINE_SKIP_UPDATES,
    CONF_ON_COMMAND_SENT,
    CONF_ON_OFFLINE,
    CONF_ON_ONLINE,
    CONF_REGISTER_COUNT,
    CONF_REGISTER_TYPE,
    CONF_RESPONSE_SIZE,
    CONF_SERVER_COURTESY_RESPONSE,
    CONF_SERVER_REGISTERS,
    CONF_SKIP_UPDATES,
    CONF_VALUE_TYPE,
)

CODEOWNERS = ["@martgras"]

AUTO_LOAD = ["modbus"]

MULTI_CONF = True

modbus_controller_ns = cg.esphome_ns.namespace("modbus_controller")
ModbusController = modbus_controller_ns.class_(
    "ModbusController", cg.PollingComponent, modbus.ModbusDevice
)

SensorItem = modbus_controller_ns.struct("SensorItem")

_LOGGER = logging.getLogger(__name__)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModbusController),
            cv.Optional(CONF_ALLOW_DUPLICATE_COMMANDS, default=False): cv.boolean,
            cv.Optional(
                CONF_COMMAND_THROTTLE, default="0ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_SERVER_COURTESY_RESPONSE): cv.invalid(
                "This option has been removed. Use modbus_server component instead: https://esphome.io/components/modbus_server/"
            ),
            cv.Optional(CONF_MAX_CMD_RETRIES, default=4): cv.positive_int,
            cv.Optional(CONF_OFFLINE_SKIP_UPDATES, default=0): cv.positive_int,
            cv.Optional(
                CONF_SERVER_REGISTERS,
            ): cv.invalid(
                "This option has been removed. Use modbus_server component instead: https://esphome.io/components/modbus_server/"
            ),
            cv.Optional(CONF_ON_COMMAND_SENT): automation.validate_automation({}),
            cv.Optional(CONF_ON_ONLINE): automation.validate_automation({}),
            cv.Optional(CONF_ON_OFFLINE): automation.validate_automation({}),
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


def _final_validate(config):
    return modbus.final_validate_modbus_device("modbus_controller", role="client")(
        config
    )


FINAL_VALIDATE_SCHEMA = _final_validate


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
        config[CONF_REGISTER_TYPE] = cv.enum(MODBUS_REGISTER_TYPE)("custom")
        config[CONF_FORCE_NEW_RANGE] = True
    return byte_offset, reg_count


async def add_modbus_base_properties(
    var, config, sensor_type, lambda_param_type=cg.float_, lambda_return_type=float
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
                (lambda_param_type, "x"),
                (
                    cg.std_vector.template(cg.uint8).operator("const").operator("ref"),
                    "data",
                ),
            ],
            return_type=cg.optional.template(lambda_return_type),
        )
        cg.add(var.set_template(template_))


_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_COMMAND_SENT,
        "add_on_command_sent_callback",
        [(cg.int_, "function_code"), (cg.int_, "address")],
    ),
    automation.CallbackAutomation(
        CONF_ON_ONLINE,
        "add_on_online_callback",
        [(cg.int_, "function_code"), (cg.int_, "address")],
    ),
    automation.CallbackAutomation(
        CONF_ON_OFFLINE,
        "add_on_offline_callback",
        [(cg.int_, "function_code"), (cg.int_, "address")],
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_allow_duplicate_commands(config[CONF_ALLOW_DUPLICATE_COMMANDS]))
    cg.add(var.set_command_throttle(config[CONF_COMMAND_THROTTLE]))
    cg.add(var.set_max_cmd_retries(config[CONF_MAX_CMD_RETRIES]))
    cg.add(var.set_offline_skip_updates(config[CONF_OFFLINE_SKIP_UPDATES]))
    await register_modbus_device(var, config)
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


async def register_modbus_device(var, config):
    cg.add(var.set_address(config[CONF_ADDRESS]))
    await cg.register_component(var, config)
    return await modbus.register_modbus_device(var, config)


def function_code_to_register(function_code):
    FUNCTION_CODE_TYPE_MAP = {
        "read_coils": ModbusRegisterType.COIL,
        "read_discrete_inputs": ModbusRegisterType.DISCRETE_INPUT,
        "read_holding_registers": ModbusRegisterType.HOLDING,
        "read_input_registers": ModbusRegisterType.READ,
        "write_single_coil": ModbusRegisterType.COIL,
        "write_single_register": ModbusRegisterType.HOLDING,
        "write_multiple_coils": ModbusRegisterType.COIL,
        "write_multiple_registers": ModbusRegisterType.HOLDING,
    }
    return FUNCTION_CODE_TYPE_MAP[function_code]
