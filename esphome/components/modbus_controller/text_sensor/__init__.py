from esphome.components import text_sensor
import esphome.config_validation as cv
import esphome.codegen as cg


from esphome.const import CONF_ID, CONF_ADDRESS, CONF_OFFSET
from .. import (
    SensorItem,
    modbus_controller_ns,
    ModbusController,
    MODBUS_FUNCTION_CODE,
)
from ..const import (
    CONF_BYTE_OFFSET,
    CONF_FORCE_NEW_RANGE,
    CONF_MODBUS_CONTROLLER_ID,
    CONF_MODBUS_FUNCTIONCODE,
    CONF_REGISTER_COUNT,
    CONF_RESPONSE_SIZE,
    CONF_SKIP_UPDATES,
    CONF_RAW_ENCODE,
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
    text_sensor.TEXT_SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ModbusTextSensor),
            cv.GenerateID(CONF_MODBUS_CONTROLLER_ID): cv.use_id(ModbusController),
            cv.Required(CONF_MODBUS_FUNCTIONCODE): cv.enum(MODBUS_FUNCTION_CODE),
            cv.Required(CONF_ADDRESS): cv.positive_int,
            cv.Optional(CONF_OFFSET, default=0): cv.positive_int,
            cv.Optional(CONF_BYTE_OFFSET): cv.positive_int,
            cv.Optional(CONF_REGISTER_COUNT, default=0): cv.positive_int,
            cv.Optional(CONF_RESPONSE_SIZE, default=2): cv.positive_int,
            cv.Optional(CONF_RAW_ENCODE, default="NONE"): cv.enum(RAW_ENCODING),
            cv.Optional(CONF_SKIP_UPDATES, default=0): cv.positive_int,
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
    response_size = config[CONF_RESPONSE_SIZE]
    reg_count = config[CONF_REGISTER_COUNT]
    if reg_count == 0:
        reg_count = response_size / 2
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_MODBUS_FUNCTIONCODE],
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
