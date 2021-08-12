from esphome.components import binary_sensor
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_ID, CONF_ADDRESS, CONF_OFFSET, CONF_NAME
from . import (
    modbus_controller_ns,
    ModbusController,
    MODBUS_FUNCTION_CODE,
    CONF_BITMASK,
)
from .const import (
    CONF_MODBUS_CONTROLLER_ID,
    CONF_MODBUS_FUNCTIONCODE,
    CONF_SKIP_UPDATES,
)

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras"]


ModbusBinarySensor = modbus_controller_ns.class_(
    "ModbusBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ModbusBinarySensor),
        cv.GenerateID(CONF_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
        cv.Required(CONF_MODBUS_FUNCTIONCODE): cv.enum(MODBUS_FUNCTION_CODE),
        cv.Required(CONF_ADDRESS): cv.int_,
        cv.Optional(CONF_OFFSET, default=0): cv.int_,
        cv.Optional(CONF_BITMASK, default=0x1): cv.hex_uint32_t,
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
        config[CONF_SKIP_UPDATES],
    )
    yield cg.register_component(var, config)
    yield binary_sensor.register_binary_sensor(var, config)

    paren = yield cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    cg.add(var.set_modbus_parent(paren))
    cg.add(
        var.add_to_controller(
            paren,
            config[CONF_MODBUS_FUNCTIONCODE],
            config[CONF_ADDRESS],
            config[CONF_OFFSET],
            config[CONF_BITMASK],
            config[CONF_SKIP_UPDATES],
        )
    )
