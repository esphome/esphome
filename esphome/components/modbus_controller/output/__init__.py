import logging

import esphome.codegen as cg
from esphome.components import output
from esphome.components.modbus.helpers import (
    SENSOR_VALUE_TYPE,
    PduBuffer,
    RegisterValues,
)
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_MULTIPLY
from esphome.types import ConfigType

from .. import (
    ModbusItemBaseSchema,
    SensorItem,
    modbus_calc_properties,
    modbus_controller_ns,
    reject_odd_holding_write_offset,
)
from ..const import (
    CONF_CUSTOM_COMMAND,
    CONF_CUSTOM_PDU,
    CONF_FORCE_NEW_RANGE,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_REGISTER_COUNT,
    CONF_REGISTER_TYPE,
    CONF_USE_WRITE_MULTIPLE,
    CONF_VALUE_TYPE,
    CONF_WRITE_LAMBDA,
)

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras"]


ModbusFloatOutput = modbus_controller_ns.class_(
    "ModbusFloatOutput", cg.Component, output.FloatOutput, SensorItem
)
ModbusBinaryOutput = modbus_controller_ns.class_(
    "ModbusBinaryOutput", cg.Component, output.BinaryOutput, SensorItem
)


def _reject_range_options(config: ConfigType) -> ConfigType:
    # Outputs are write-only and never polled; the range options have no effect on them.
    for key in (CONF_FORCE_NEW_RANGE, CONF_REGISTER_COUNT):
        if config.pop(key, None) is not None:
            _LOGGER.warning(
                "%s: '%s' has no effect on outputs; remove it. Removed in 2027.3.0",
                config.get(CONF_ID),
                key,
            )
    return config


CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            "coil": output.BINARY_OUTPUT_SCHEMA.extend(ModbusItemBaseSchema).extend(
                {
                    cv.GenerateID(): cv.declare_id(ModbusBinaryOutput),
                    cv.Required(CONF_ADDRESS): cv.positive_int,
                    cv.Optional(CONF_CUSTOM_PDU): cv.invalid(
                        "custom_pdu is not supported for outputs; use a write_lambda instead"
                    ),
                    cv.Optional(CONF_CUSTOM_COMMAND): cv.invalid(
                        "custom_command is not supported for outputs; use a write_lambda instead"
                    ),
                    cv.Optional(CONF_WRITE_LAMBDA): cv.returning_lambda,
                    cv.Optional(CONF_USE_WRITE_MULTIPLE, default=False): cv.boolean,
                }
            ),
            "holding": cv.All(
                output.FLOAT_OUTPUT_SCHEMA.extend(ModbusItemBaseSchema).extend(
                    {
                        cv.GenerateID(): cv.declare_id(ModbusFloatOutput),
                        cv.Required(CONF_ADDRESS): cv.positive_int,
                        cv.Optional(CONF_CUSTOM_PDU): cv.invalid(
                            "custom_pdu is not supported for outputs; use a write_lambda instead"
                        ),
                        cv.Optional(CONF_CUSTOM_COMMAND): cv.invalid(
                            "custom_command is not supported for outputs; use a write_lambda instead"
                        ),
                        cv.Optional(CONF_VALUE_TYPE, default="U_WORD"): cv.enum(
                            SENSOR_VALUE_TYPE
                        ),
                        cv.Optional(CONF_WRITE_LAMBDA): cv.returning_lambda,
                        cv.Optional(CONF_MULTIPLY, default=1.0): cv.float_,
                        cv.Optional(CONF_USE_WRITE_MULTIPLE, default=False): cv.boolean,
                    }
                ),
                reject_odd_holding_write_offset,
            ),
        },
        lower=True,
        key=CONF_REGISTER_TYPE,
        default_type="holding",
    ),
    _reject_range_options,
)


async def to_code(config: ConfigType) -> None:
    byte_offset = modbus_calc_properties(config)
    # Binary Output
    write_template = None
    if config[CONF_REGISTER_TYPE] == "coil":
        var = cg.new_Pvariable(
            config[CONF_ID],
            config[CONF_ADDRESS],
            byte_offset,
        )
        if CONF_WRITE_LAMBDA in config:
            write_template = await cg.process_lambda(
                config[CONF_WRITE_LAMBDA],
                [
                    (ModbusBinaryOutput.operator("ptr"), "item"),
                    (cg.bool_, "x"),
                    (PduBuffer.operator("ref"), "payload"),
                ],
                return_type=cg.optional.template(bool),
            )
    # Float Output
    else:
        var = cg.new_Pvariable(
            config[CONF_ID],
            config[CONF_ADDRESS],
            byte_offset,
            config[CONF_VALUE_TYPE],
        )
        cg.add(var.set_write_multiply(config[CONF_MULTIPLY]))
        if CONF_WRITE_LAMBDA in config:
            write_template = await cg.process_lambda(
                config[CONF_WRITE_LAMBDA],
                [
                    (ModbusFloatOutput.operator("ptr"), "item"),
                    (cg.float_, "x"),
                    (RegisterValues.operator("ref"), "payload"),
                ],
                return_type=cg.optional.template(float),
            )
    await output.register_output(var, config)
    parent = await cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    cg.add(var.set_use_write_mutiple(config[CONF_USE_WRITE_MULTIPLE]))
    cg.add(var.set_parent(parent))
    if write_template:
        cg.add(var.set_write_template(write_template))
