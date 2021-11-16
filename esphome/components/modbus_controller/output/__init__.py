import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output

from esphome.const import (
    CONF_ADDRESS,
    CONF_ID,
    CONF_MULTIPLY,
    CONF_OFFSET,
)

from .. import (
    SensorItem,
    modbus_controller_ns,
    ModbusController,
    TYPE_REGISTER_MAP,
)

from ..const import (
    CONF_BYTE_OFFSET,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_REGISTER_COUNT,
    CONF_VALUE_TYPE,
    CONF_WRITE_LAMBDA,
)
from ..sensor import SENSOR_VALUE_TYPE

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras"]


ModbusOutput = modbus_controller_ns.class_(
    "ModbusOutput", cg.Component, output.FloatOutput, SensorItem
)

CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
            cv.GenerateID(): cv.declare_id(ModbusOutput),
            cv.Required(CONF_ADDRESS): cv.positive_int,
            cv.Optional(CONF_OFFSET, default=0): cv.positive_int,
            cv.Optional(CONF_BYTE_OFFSET): cv.positive_int,
            cv.Optional(CONF_VALUE_TYPE, default="U_WORD"): cv.enum(SENSOR_VALUE_TYPE),
            cv.Optional(CONF_REGISTER_COUNT, default=0): cv.positive_int,
            cv.Optional(CONF_WRITE_LAMBDA): cv.returning_lambda,
            cv.Optional(CONF_MULTIPLY, default=1.0): cv.float_,
        }
    ),
)


async def to_code(config):
    byte_offset = 0
    if CONF_OFFSET in config:
        byte_offset = config[CONF_OFFSET]
    # A CONF_BYTE_OFFSET setting overrides CONF_OFFSET
    if CONF_BYTE_OFFSET in config:
        byte_offset = config[CONF_BYTE_OFFSET]
    value_type = config[CONF_VALUE_TYPE]
    reg_count = config[CONF_REGISTER_COUNT]
    if reg_count == 0:
        reg_count = TYPE_REGISTER_MAP[value_type]
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_ADDRESS],
        byte_offset,
        value_type,
        reg_count,
    )
    await output.register_output(var, config)
    cg.add(var.set_write_multiply(config[CONF_MULTIPLY]))
    parent = await cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    cg.add(var.set_parent(parent))
    if CONF_WRITE_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_WRITE_LAMBDA],
            [
                (ModbusOutput.operator("ptr"), "item"),
                (cg.float_, "x"),
                (cg.std_vector.template(cg.uint16).operator("ref"), "payload"),
            ],
            return_type=cg.optional.template(float),
        )
        cg.add(var.set_write_template(template_))
