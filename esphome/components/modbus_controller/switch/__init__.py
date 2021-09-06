from esphome.components import switch
import esphome.config_validation as cv
import esphome.codegen as cg


from esphome.const import CONF_ID, CONF_ADDRESS, CONF_OFFSET
from .. import SensorItem, modbus_controller_ns, ModbusController, MODBUS_FUNCTION_CODE
from ..const import (
    CONF_BITMASK,
    CONF_BYTE_OFFSET,
    CONF_FORCE_NEW_RANGE,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_MODBUS_FUNCTIONCODE,
)

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras"]


ModbusSwitch = modbus_controller_ns.class_(
    "ModbusSwitch", cg.Component, switch.Switch, SensorItem
)


CONFIG_SCHEMA = cv.All(
    switch.SWITCH_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ModbusSwitch),
            cv.GenerateID(CONF_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
            cv.Required(CONF_MODBUS_FUNCTIONCODE): cv.enum(MODBUS_FUNCTION_CODE),
            cv.Required(CONF_ADDRESS): cv.positive_int,
            cv.Optional(CONF_OFFSET, default=0): cv.positive_int,
            cv.Optional(CONF_BYTE_OFFSET): cv.positive_int,
            cv.Optional(CONF_BITMASK, default=0x1): cv.hex_uint32_t,
            cv.Optional(CONF_FORCE_NEW_RANGE, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
)


async def to_code(config):
    byte_offset = 0
    if CONF_OFFSET in config:
        byte_offset = config[CONF_OFFSET] * 2
    # A CONF_BYTE_OFFSET setting overrides CONF_OFFSET
    if CONF_BYTE_OFFSET in config:
        byte_offset = config[CONF_BYTE_OFFSET]
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_MODBUS_FUNCTIONCODE],
        config[CONF_ADDRESS],
        byte_offset,
        config[CONF_BITMASK],
        config[CONF_FORCE_NEW_RANGE],
    )
    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    paren = await cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    cg.add(paren.add_sensor_item(var))
    cg.add(var.set_parent(paren))
