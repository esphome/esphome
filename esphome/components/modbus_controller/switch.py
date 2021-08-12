from esphome.components import switch
import esphome.config_validation as cv
import esphome.codegen as cg


from esphome.const import CONF_ID, CONF_ADDRESS, CONF_OFFSET
from . import modbus_controller_ns, ModbusController, MODBUS_FUNCTION_CODE
from .const import CONF_MODBUS_CONTROLLER_ID, CONF_MODBUS_FUNCTIONCODE, CONF_BITMASK

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras"]


ModbusSwitch = modbus_controller_ns.class_("ModbusSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ModbusSwitch),
        cv.GenerateID(CONF_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
        cv.Required(CONF_MODBUS_FUNCTIONCODE): cv.enum(MODBUS_FUNCTION_CODE),
        cv.Required(CONF_ADDRESS): cv.int_,
        cv.Optional(CONF_OFFSET, default=0): cv.int_,
        cv.Optional(CONF_BITMASK, default=0x1): cv.hex_uint32_t,
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_MODBUS_FUNCTIONCODE],
        config[CONF_ADDRESS],
        config[CONF_OFFSET],
        config[CONF_BITMASK],
    )
    yield cg.register_component(var, config)
    yield switch.register_switch(var, config)

    paren = yield cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    cg.add(
        var.add_to_controller(
            paren,
            config[CONF_MODBUS_FUNCTIONCODE],
            config[CONF_ADDRESS],
            config[CONF_OFFSET],
            config[CONF_BITMASK],
        )
    )
