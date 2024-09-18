import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MULTIPLY,
    CONF_STEP,
)

from .. import (
    MODBUS_WRITE_REGISTER_TYPE,
    SENSOR_VALUE_TYPE,
    ModbusItemBaseSchema,
    SensorItem,
    add_modbus_base_properties,
    modbus_calc_properties,
    modbus_controller_ns,
)
from ..const import (
    CONF_BITMASK,
    CONF_CUSTOM_COMMAND,
    CONF_FORCE_NEW_RANGE,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_REGISTER_TYPE,
    CONF_SKIP_UPDATES,
    CONF_USE_WRITE_MULTIPLE,
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


def validate_modbus_number(config):
    if CONF_CUSTOM_COMMAND not in config and CONF_ADDRESS not in config:
        raise cv.Invalid(
            f" {CONF_ADDRESS} is a required property if '{CONF_CUSTOM_COMMAND}:' isn't used"
        )
    return config


CONFIG_SCHEMA = cv.All(
    number.number_schema(ModbusNumber)
    .extend(ModbusItemBaseSchema)
    .extend(
        {
            cv.Optional(CONF_REGISTER_TYPE, default="holding"): cv.enum(
                MODBUS_WRITE_REGISTER_TYPE
            ),
            cv.Optional(CONF_VALUE_TYPE, default="U_WORD"): cv.enum(SENSOR_VALUE_TYPE),
            cv.Optional(CONF_WRITE_LAMBDA): cv.returning_lambda,
            # 24 bits are the maximum value for fp32 before precision is lost
            # 0x00FFFFFF = 16777215
            cv.Optional(CONF_MAX_VALUE, default=16777215.0): cv.float_,
            cv.Optional(CONF_MIN_VALUE, default=-16777215.0): cv.float_,
            cv.Optional(CONF_STEP, default=1): cv.positive_float,
            cv.Optional(CONF_MULTIPLY, default=1.0): cv.float_,
            cv.Optional(CONF_USE_WRITE_MULTIPLE, default=False): cv.boolean,
        }
    ),
    validate_min_max,
    validate_modbus_number,
)


async def to_code(config):
    byte_offset, reg_count = modbus_calc_properties(config)
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_REGISTER_TYPE],
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
    await add_modbus_base_properties(var, config, ModbusNumber)
    cg.add(var.set_use_write_mutiple(config[CONF_USE_WRITE_MULTIPLE]))
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
