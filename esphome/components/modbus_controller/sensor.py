from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_ID, CONF_ADDRESS, CONF_OFFSET, CONF_NAME
from . import (
    modbus_controller_ns,
    ModbusController,
    MODBUS_FUNCTION_CODE,
    CONF_BITMASK,
    CONF_VALUE_TYPE,
    CONF_REGISTER_COUNT,
    SENSOR_VALUE_TYPE,
)
from .const import (
    CONF_MODBUS_CONTROLLER_ID,
    CONF_MODBUS_FUNCTIONCODE,
    CONF_SKIP_UPDATES,
)

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras"]


ModbusSensor = modbus_controller_ns.class_("ModbusSensor", sensor.Sensor, cg.Component)

CONFIG_SCHEMA = sensor.SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ModbusSensor),
        cv.GenerateID(CONF_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
        cv.Required(CONF_MODBUS_FUNCTIONCODE): cv.enum(MODBUS_FUNCTION_CODE),
        cv.Required(CONF_ADDRESS): cv.int_,
        cv.Optional(CONF_OFFSET, default=0): cv.int_,
        cv.Optional(CONF_BITMASK, default=0xFFFFFFFF): cv.hex_uint32_t,
        cv.Optional(CONF_VALUE_TYPE, default="U_WORD"): cv.enum(SENSOR_VALUE_TYPE),
        cv.Optional(CONF_REGISTER_COUNT, default=1): cv.int_,
        cv.Optional(CONF_SKIP_UPDATES, default=0): cv.int_,
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_NAME],
        config[CONF_MODBUS_FUNCTIONCODE],
        config[CONF_ADDRESS],
        config[CONF_OFFSET],
        config[CONF_BITMASK],
        config[CONF_VALUE_TYPE],
        config[CONF_REGISTER_COUNT],
        config[CONF_SKIP_UPDATES],
    )
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)

    paren = yield cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    cg.add(
        var.add_to_controller(
            paren,
            config[CONF_MODBUS_FUNCTIONCODE],
            config[CONF_ADDRESS],
            config[CONF_OFFSET],
            config[CONF_BITMASK],
            config[CONF_VALUE_TYPE],
            config[CONF_REGISTER_COUNT],
            config[CONF_SKIP_UPDATES],
        )
    )
