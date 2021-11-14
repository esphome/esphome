import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_ADDRESS,
    CONF_ID,
    CONF_LAMBDA,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MULTIPLY,
    CONF_OFFSET,
    CONF_STEP,
)

from .. import (
    modbus_controller_ns,
    ModbusController,
    SENSOR_VALUE_TYPE,
    SensorItem,
    TYPE_REGISTER_MAP,
)


from ..const import (
    CONF_BITMASK,
    CONF_BYTE_OFFSET,
    CONF_FORCE_NEW_RANGE,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_REGISTER_COUNT,
    CONF_SKIP_UPDATES,
    CONF_VALUE_TYPE,
    CONF_WRITE_LAMBDA,
)

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras"]


ModbusNumber = modbus_controller_ns.class_(
    "ModbusNumber", cg.Component, number.Number, SensorItem
)


def validate_min_max(config):
    if config[CONF_MAX_VALUE] <= config[CONF_MIN_VALUE]:
        raise cv.Invalid("max_value must be greater than min_value")
    if config[CONF_MIN_VALUE] < -16777215:
        raise cv.Invalid("max_value must be greater than -16777215")
    if config[CONF_MAX_VALUE] > 16777215:
        raise cv.Invalid("max_value must not be greater than 16777215")
    return config


CONFIG_SCHEMA = cv.All(
    number.NUMBER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ModbusNumber),
            cv.GenerateID(CONF_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
            cv.Required(CONF_ADDRESS): cv.positive_int,
            cv.Optional(CONF_OFFSET, default=0): cv.positive_int,
            cv.Optional(CONF_BYTE_OFFSET): cv.positive_int,
            cv.Optional(CONF_BITMASK, default=0xFFFFFFFF): cv.hex_uint32_t,
            cv.Optional(CONF_VALUE_TYPE, default="U_WORD"): cv.enum(SENSOR_VALUE_TYPE),
            cv.Optional(CONF_REGISTER_COUNT, default=0): cv.positive_int,
            cv.Optional(CONF_SKIP_UPDATES, default=0): cv.positive_int,
            cv.Optional(CONF_FORCE_NEW_RANGE, default=False): cv.boolean,
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
            cv.Optional(CONF_WRITE_LAMBDA): cv.returning_lambda,
            cv.GenerateID(): cv.declare_id(ModbusNumber),
            # 24 bits are the maximum value for fp32 before precison is lost
            # 0x00FFFFFF = 16777215
            cv.Optional(CONF_MAX_VALUE, default=16777215.0): cv.float_,
            cv.Optional(CONF_MIN_VALUE, default=-16777215.0): cv.float_,
            cv.Optional(CONF_STEP, default=1): cv.positive_float,
            cv.Optional(CONF_MULTIPLY, default=1.0): cv.float_,
        }
    ).extend(cv.polling_component_schema("60s")),
    validate_min_max,
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
        config[CONF_BITMASK],
        config[CONF_VALUE_TYPE],
        reg_count,
        config[CONF_SKIP_UPDATES],
        config[CONF_FORCE_NEW_RANGE],
    )

    await cg.register_component(var, config)
    await number.register_number(
        var,
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )

    cg.add(var.set_write_multiply(config[CONF_MULTIPLY]))
    parent = await cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])

    cg.add(var.set_parent(parent))
    cg.add(parent.add_sensor_item(var))
    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [
                (ModbusNumber.operator("ptr"), "item"),
                (cg.float_, "x"),
                (
                    cg.std_vector.template(cg.uint8).operator("const").operator("ref"),
                    "data",
                ),
            ],
            return_type=cg.optional.template(float),
        )
        cg.add(var.set_template(template_))
    if CONF_WRITE_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_WRITE_LAMBDA],
            [
                (ModbusNumber.operator("ptr"), "item"),
                (cg.float_, "x"),
                (cg.std_vector.template(cg.uint16).operator("ref"), "payload"),
            ],
            return_type=cg.optional.template(float),
        )
        cg.add(var.set_write_template(template_))
