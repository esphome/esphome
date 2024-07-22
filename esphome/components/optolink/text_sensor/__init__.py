import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_ADDRESS,
    CONF_BYTES,
    CONF_DIV_RATIO,
    CONF_ENTITY_ID,
    CONF_ID,
    CONF_TYPE,
)
from .. import (
    CONF_DAY_OF_WEEK,
    DAY_OF_WEEK,
    check_address_for_types,
    check_bytes_for_types,
    check_dow_for_types,
    optolink_ns,
    CONF_OPTOLINK_ID,
    SENSOR_BASE_SCHEMA,
)

DEPENDENCIES = ["optolink"]
CODEOWNERS = ["@j0ta29"]

TextSensorType = optolink_ns.enum("TextSensorType")
TYPE = {
    "MAP": TextSensorType.TEXT_SENSOR_TYPE_MAP,
    "RAW": TextSensorType.TEXT_SENSOR_TYPE_RAW,
    "DAY_SCHEDULE": TextSensorType.TEXT_SENSOR_TYPE_DAY_SCHEDULE,
    "DEVICE_INFO": TextSensorType.TEXT_SENSOR_TYPE_DEVICE_INFO,
    "STATE_INFO": TextSensorType.TEXT_SENSOR_TYPE_STATE_INFO,
}

OptolinkTextSensor = optolink_ns.class_(
    "OptolinkTextSensor", text_sensor.TextSensor, cg.PollingComponent
)

CONFIG_SCHEMA = cv.All(
    text_sensor.TEXT_SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(OptolinkTextSensor),
            cv.Required(CONF_TYPE): cv.enum(TYPE, upper=True),
            cv.Optional(CONF_ADDRESS): cv.hex_uint32_t,
            cv.Optional(CONF_BYTES): cv.uint8_t,
            cv.Optional(CONF_DAY_OF_WEEK): cv.enum(DAY_OF_WEEK, upper=True),
            cv.Optional(CONF_ENTITY_ID): cv.entity_id,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(SENSOR_BASE_SCHEMA),
    check_address_for_types(["MAP", "RAW", "DAY_SCHEDULE"]),
    check_bytes_for_types(["MAP", "RAW", "DAY_SCHEDULE"]),
    check_dow_for_types(["DAY_SCHEDULE"]),
)


async def to_code(config):
    component = await cg.get_variable(config[CONF_OPTOLINK_ID])
    var = cg.new_Pvariable(config[CONF_ID], component)

    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)

    cg.add(var.set_type(config[CONF_TYPE]))
    if CONF_ADDRESS in config:
        cg.add(var.set_address(config[CONF_ADDRESS]))
    if CONF_DIV_RATIO in config:
        cg.add(var.set_div_ratio(config[CONF_DIV_RATIO]))
    if CONF_BYTES in config:
        cg.add(var.set_bytes(config[CONF_BYTES]))
    if CONF_DAY_OF_WEEK in config:
        cg.add(var.set_day_of_week(config[CONF_DAY_OF_WEEK]))
    if CONF_ENTITY_ID in config:
        cg.add(var.set_entity_id(config[CONF_ENTITY_ID]))
