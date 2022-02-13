from esphome.components import text_sensor
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
    CONF_FORCE_NEW_RANGE,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_REGISTER_COUNT,
    CONF_RESPONSE_SIZE,
    CONF_SKIP_UPDATES,
    CONF_RAW_ENCODE,
    CONF_REGISTER_TYPE,
)

DEPENDENCIES = ["modbus_controller"]
CODEOWNERS = ["@martgras"]


ModbusTextSensor = modbus_controller_ns.class_(
    "ModbusTextSensor", cg.Component, text_sensor.TextSensor, SensorItem
)

RawEncoding_ns = modbus_controller_ns.namespace("RawEncoding")
RawEncoding = RawEncoding_ns.enum("RawEncoding")
RAW_ENCODING = {
    "NONE": RawEncoding.NONE,
    "HEXBYTES": RawEncoding.HEXBYTES,
    "COMMA": RawEncoding.COMMA,
}

CONFIG_SCHEMA = cv.All(
    text_sensor.text_sensor_schema()
    .extend(cv.COMPONENT_SCHEMA)
    .extend(ModbusItemBaseSchema)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(ModbusTextSensor),
            cv.Optional(CONF_REGISTER_TYPE): cv.enum(MODBUS_REGISTER_TYPE),
            cv.Optional(CONF_REGISTER_COUNT, default=0): cv.positive_int,
            cv.Optional(CONF_RESPONSE_SIZE, default=2): cv.positive_int,
            cv.Optional(CONF_RAW_ENCODE, default="NONE"): cv.enum(RAW_ENCODING),
        }
    ),
    validate_modbus_register,
)


async def to_code(config):
    byte_offset, reg_count = modbus_calc_properties(config)
    response_size = config[CONF_RESPONSE_SIZE]
    reg_count = config[CONF_REGISTER_COUNT]
    if reg_count == 0:
        reg_count = response_size / 2
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_REGISTER_TYPE],
        config[CONF_ADDRESS],
        byte_offset,
        reg_count,
        config[CONF_RESPONSE_SIZE],
        config[CONF_RAW_ENCODE],
        config[CONF_SKIP_UPDATES],
        config[CONF_FORCE_NEW_RANGE],
    )

    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)

    paren = await cg.get_variable(config[CONF_MODBUS_CONTROLLER_ID])
    cg.add(paren.add_sensor_item(var))
    await add_modbus_base_properties(
        var, config, ModbusTextSensor, cg.std_string, cg.std_string
    )
