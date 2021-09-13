import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.components.modbus_controller.sensor import ModbusSensor
from esphome.const import (
    CONF_ID,
    CONF_LAMBDA,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MULTIPLY,
    CONF_STEP,
)

from .. import (
    SensorItem,
    modbus_controller_ns,
    ModbusController,
)

from ..const import (
    CONF_MODBUS_CONTROLLER_ID,
)

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras"]


ModbusNumber = modbus_controller_ns.class_(
    "ModbusNumber", cg.Component, number.Number, SensorItem
)

CONF_MODBUS_SENSOR_ID = "modbus_sensor_id"


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
            # 24 bits are the maximum value for fp32 before precison is lost
            # 0x00FFFFFF = 16777215
            cv.Optional(CONF_MAX_VALUE, default=16777215.0): cv.float_,
            cv.Optional(CONF_MIN_VALUE, default=-16777215.0): cv.float_,
            cv.Optional(CONF_STEP, default=1): cv.positive_float,
            cv.Optional(CONF_LAMBDA): cv.returning_lambda,
            cv.GenerateID(CONF_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
            cv.GenerateID(CONF_MODBUS_SENSOR_ID): cv.use_id(ModbusSensor),
            cv.Optional(CONF_MULTIPLY, default=1.0): cv.float_,
        }
    ).extend(cv.polling_component_schema("60s")),
    validate_min_max,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await number.register_number(
        var,
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )

    cg.add(var.set_multiply(config[CONF_MULTIPLY]))
    parent = await cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    sensor = await cg.get_variable(config[CONF_MODBUS_SENSOR_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_sensor(sensor))

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [
                (cg.float_, "x"),
                (cg.std_vector.template(cg.uint16).operator("ref"), "payload"),
            ],
            return_type=cg.optional.template(float),
        )
        cg.add(var.set_template(template_))
        cg.add(var.set_template(template_))
