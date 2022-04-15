import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import (
    CONF_ADDRESS,
    CONF_ID,
    CONF_MULTIPLY,
)

from .. import (
    modbus_controller_ns,
    modbus_calc_properties,
    ModbusItemBaseSchema,
    SensorItem,
    SENSOR_VALUE_TYPE,
)

from ..const import (
    CONF_MODBUS_CONTROLLER_ID,
    CONF_REGISTER_TYPE,
    CONF_USE_WRITE_MULTIPLE,
    CONF_VALUE_TYPE,
    CONF_WRITE_LAMBDA,
)

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras"]


ModbusFloatOutput = modbus_controller_ns.class_(
    "ModbusFloatOutput", cg.Component, output.FloatOutput, SensorItem
)
ModbusBinaryOutput = modbus_controller_ns.class_(
    "ModbusBinaryOutput", cg.Component, output.BinaryOutput, SensorItem
)


CONFIG_SCHEMA = cv.typed_schema(
    {
        "coil": output.BINARY_OUTPUT_SCHEMA.extend(ModbusItemBaseSchema).extend(
            {
                cv.GenerateID(): cv.declare_id(ModbusBinaryOutput),
                cv.Optional(CONF_WRITE_LAMBDA): cv.returning_lambda,
                cv.Optional(CONF_USE_WRITE_MULTIPLE, default=False): cv.boolean,
            }
        ),
        "holding": output.FLOAT_OUTPUT_SCHEMA.extend(ModbusItemBaseSchema).extend(
            {
                cv.GenerateID(): cv.declare_id(ModbusFloatOutput),
                cv.Optional(CONF_VALUE_TYPE, default="U_WORD"): cv.enum(
                    SENSOR_VALUE_TYPE
                ),
                cv.Optional(CONF_WRITE_LAMBDA): cv.returning_lambda,
                cv.Optional(CONF_MULTIPLY, default=1.0): cv.float_,
                cv.Optional(CONF_USE_WRITE_MULTIPLE, default=False): cv.boolean,
            }
        ),
    },
    lower=True,
    key=CONF_REGISTER_TYPE,
    default_type="holding",
)


async def to_code(config):
    byte_offset, reg_count = modbus_calc_properties(config)
    # Binary Output
    if config[CONF_REGISTER_TYPE] == "coil":
        var = cg.new_Pvariable(
            config[CONF_ID],
            config[CONF_ADDRESS],
            byte_offset,
        )
        if CONF_WRITE_LAMBDA in config:
            template_ = await cg.process_lambda(
                config[CONF_WRITE_LAMBDA],
                [
                    (ModbusBinaryOutput.operator("ptr"), "item"),
                    (cg.bool_, "x"),
                    (cg.std_vector.template(cg.uint8).operator("ref"), "payload"),
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
            reg_count,
        )
        cg.add(var.set_write_multiply(config[CONF_MULTIPLY]))
        if CONF_WRITE_LAMBDA in config:
            template_ = await cg.process_lambda(
                config[CONF_WRITE_LAMBDA],
                [
                    (ModbusFloatOutput.operator("ptr"), "item"),
                    (cg.float_, "x"),
                    (cg.std_vector.template(cg.uint16).operator("ref"), "payload"),
                ],
                return_type=cg.optional.template(float),
            )
    await output.register_output(var, config)
    parent = await cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    cg.add(var.set_use_write_mutiple(config[CONF_USE_WRITE_MULTIPLE]))
    cg.add(var.set_parent(parent))
    if CONF_WRITE_LAMBDA in config:
        cg.add(var.set_write_template(template_))
