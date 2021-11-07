from esphome.components import switch
import esphome.config_validation as cv
import esphome.codegen as cg


from esphome.const import CONF_ID, CONF_ADDRESS
from .. import (
    add_modbus_base_properties,
    modbus_controller_ns,
    modbus_calc_properties,
    validate_register_type,
    ModbusItemBaseSchema,
    SensorItem,
    MODBUS_REGISTER_TYPE,
)
from ..const import (
    CONF_BITMASK,
    CONF_FORCE_NEW_RANGE,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_REGISTER_TYPE,
)

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras"]


ModbusSwitch = modbus_controller_ns.class_(
    "ModbusSwitch", cg.Component, switch.Switch, SensorItem
)


CONFIG_SCHEMA = cv.All(
    switch.SWITCH_SCHEMA.extend(
        ModbusItemBaseSchema.extend(
            {
                cv.GenerateID(): cv.declare_id(ModbusSwitch),
                cv.Optional(CONF_REGISTER_TYPE): cv.enum(MODBUS_REGISTER_TYPE),
            }
        )
    ).extend(cv.COMPONENT_SCHEMA),
    validate_register_type,
)


async def to_code(config):
    byte_offset, _ = modbus_calc_properties(config)
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_REGISTER_TYPE],
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
    await add_modbus_base_properties(var, config, ModbusSwitch, bool, bool)
