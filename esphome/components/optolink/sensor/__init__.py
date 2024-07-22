import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ADDRESS,
    CONF_BYTES,
    CONF_DIV_RATIO,
    CONF_ID,
    CONF_MIN_VALUE,
    CONF_TYPE,
)
from .. import (
    CONF_OPTOLINK_ID,
    SENSOR_BASE_SCHEMA,
    check_address_for_types,
    check_bytes_for_types,
    optolink_ns,
)

DEPENDENCIES = ["optolink"]
CODEOWNERS = ["@j0ta29"]

SensorType = optolink_ns.enum("SensorType")
TYPE = {
    "DATAPOINT": SensorType.SENSOR_TYPE_DATAPOINT,
    "QUEUE_SIZE": SensorType.SENSOR_TYPE_QUEUE_SIZE,
}

OptolinkSensor = optolink_ns.class_(
    "OptolinkSensor", sensor.Sensor, cg.PollingComponent
)
CONFIG_SCHEMA = cv.All(
    sensor.SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(OptolinkSensor),
            cv.Optional(CONF_TYPE, default="DATAPOINT"): cv.enum(TYPE, upper=True),
            cv.Optional(CONF_ADDRESS): cv.hex_uint32_t,
            cv.Optional(CONF_BYTES): cv.one_of(1, 2, 4, int=True),
            cv.Optional(CONF_MIN_VALUE): cv.float_,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(SENSOR_BASE_SCHEMA),
    check_address_for_types(["DATAPOINT"]),
    check_bytes_for_types(["DATAPOINT"]),
)


async def to_code(config):
    component = await cg.get_variable(config[CONF_OPTOLINK_ID])
    var = cg.new_Pvariable(config[CONF_ID], component)

    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    cg.add(var.set_type(config[CONF_TYPE]))
    if CONF_ADDRESS in config:
        cg.add(var.set_address(config[CONF_ADDRESS]))
    if CONF_BYTES in config:
        cg.add(var.set_bytes(config[CONF_BYTES]))
    if CONF_DIV_RATIO in config:
        cg.add(var.set_div_ratio(config[CONF_DIV_RATIO]))
    if CONF_MIN_VALUE in config:
        cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
