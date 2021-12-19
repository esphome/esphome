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
    validate_modbus_register,
    ModbusItemBaseSchema,
    SensorItem,
)

from ..const import (
    CONF_MODBUS_CONTROLLER_ID,
    CONF_USE_WRITE_MULTIPLE,
    CONF_VALUE_TYPE,
    CONF_WRITE_LAMBDA,
)

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras"]


ModbusOutput = modbus_controller_ns.class_(
    "ModbusOutput", cg.Component, output.FloatOutput, SensorItem
)

CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend(ModbusItemBaseSchema).extend(
        {
            cv.GenerateID(): cv.declare_id(ModbusOutput),
            cv.Optional(CONF_WRITE_LAMBDA): cv.returning_lambda,
            cv.Optional(CONF_MULTIPLY, default=1.0): cv.float_,
            cv.Optional(CONF_USE_WRITE_MULTIPLE, default=False): cv.boolean,
        }
    ),
    validate_modbus_register,
)


async def to_code(config):
    byte_offset, reg_count = modbus_calc_properties(config)
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_ADDRESS],
        byte_offset,
        config[CONF_VALUE_TYPE],
        reg_count,
    )
    await output.register_output(var, config)
    cg.add(var.set_write_multiply(config[CONF_MULTIPLY]))
    parent = await cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    cg.add(var.set_use_write_mutiple(config[CONF_USE_WRITE_MULTIPLE]))
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
