from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.const import CONF_ID, CONF_ADDRESS
from .. import (
    add_modbus_base_properties,
    modbus_controller_ns,
    modbus_calc_properties,
    validate_modbus_register,
    ModbusItemBaseSchema,
    SensorItem,
    MODBUS_REGISTER_TYPE,
    SENSOR_VALUE_TYPE,
)
from ..const import (
    CONF_BITMASK,
    CONF_FORCE_NEW_RANGE,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_REGISTER_COUNT,
    CONF_REGISTER_TYPE,
    CONF_SKIP_UPDATES,
    CONF_VALUE_TYPE,
)

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras"]


ModbusSensor = modbus_controller_ns.class_(
    "ModbusSensor", cg.Component, sensor.Sensor, SensorItem
)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(ModbusSensor)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ModbusItemBaseSchema)
    .extend(
        {
            cv.Optional(CONF_REGISTER_TYPE): cv.enum(MODBUS_REGISTER_TYPE),
            cv.Optional(CONF_VALUE_TYPE, default="U_WORD"): cv.enum(SENSOR_VALUE_TYPE),
            cv.Optional(CONF_REGISTER_COUNT, default=0): cv.positive_int,
        }
    ),
    validate_modbus_register,
)


async def to_code(config):
    byte_offset, reg_count = modbus_calc_properties(config)
    value_type = config[CONF_VALUE_TYPE]
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_REGISTER_TYPE],
        config[CONF_ADDRESS],
        byte_offset,
        config[CONF_BITMASK],
        value_type,
        reg_count,
        config[CONF_SKIP_UPDATES],
        config[CONF_FORCE_NEW_RANGE],
    )
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    paren = await cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    cg.add(paren.add_sensor_item(var))
    await add_modbus_base_properties(var, config, ModbusSensor)
