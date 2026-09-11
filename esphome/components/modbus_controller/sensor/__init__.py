import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.modbus.helpers import MODBUS_REGISTER_TYPE, SENSOR_VALUE_TYPE
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID

from .. import (
    RANGE_REUSE,
    ModbusItemBaseSchema,
    SensorItem,
    add_modbus_base_properties,
    modbus_calc_properties,
    modbus_controller_ns,
    validate_custom_pdu_item,
    validate_modbus_register,
    validate_range_reuse_migration,
)
from ..const import (
    CONF_BITMASK,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_REGISTER_TYPE,
    CONF_REUSE_PREVIOUS_RANGE,
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
        }
    ),
    validate_modbus_register,
    validate_range_reuse_migration,
)

FINAL_VALIDATE_SCHEMA = validate_custom_pdu_item


async def to_code(config):
    byte_offset = modbus_calc_properties(config)
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_REGISTER_TYPE],
        config[CONF_ADDRESS],
        byte_offset,
        config[CONF_BITMASK],
        config[CONF_VALUE_TYPE],
        RANGE_REUSE[config[CONF_REUSE_PREVIOUS_RANGE]],
    )
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    paren = await cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    cg.add(paren.add_sensor_item(var))
    await add_modbus_base_properties(var, config, ModbusSensor)
