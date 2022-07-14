from esphome.components import binary_sensor
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_ADDRESS, CONF_ID
from .. import (
    add_modbus_base_properties,
    modbus_controller_ns,
    modbus_calc_properties,
    validate_modbus_register,
    ModbusItemBaseSchema,
    SensorItem,
    MODBUS_REGISTER_TYPE,
)
from ..const import (
    CONF_BITMASK,
    CONF_FORCE_NEW_RANGE,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_REGISTER_TYPE,
    CONF_SKIP_UPDATES,
)

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras"]


ModbusBinarySensor = modbus_controller_ns.class_(
    "ModbusBinarySensor", cg.Component, binary_sensor.BinarySensor, SensorItem
)

CONFIG_SCHEMA = cv.All(
    binary_sensor.binary_sensor_schema(ModbusBinarySensor)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ModbusItemBaseSchema)
    .extend(
        {
            cv.Optional(CONF_REGISTER_TYPE): cv.enum(MODBUS_REGISTER_TYPE),
        }
    ),
    validate_modbus_register,
)


async def to_code(config):
    byte_offset, _ = modbus_calc_properties(config)
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_REGISTER_TYPE],
        config[CONF_ADDRESS],
        byte_offset,
        config[CONF_BITMASK],
        config[CONF_SKIP_UPDATES],
        config[CONF_FORCE_NEW_RANGE],
    )
    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)

    paren = await cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    cg.add(paren.add_sensor_item(var))
    await add_modbus_base_properties(var, config, ModbusBinarySensor, bool, bool)
