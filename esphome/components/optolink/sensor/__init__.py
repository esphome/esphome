import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ADDRESS,
    CONF_BYTES,
    CONF_DIV_RATIO,
    CONF_ID,
    CONF_MIN_VALUE,
)
from .. import CONF_OPTOLINK_ID, SENSOR_BASE_SCHEMA, optolink_ns

DEPENDENCIES = ["optolink"]
CODEOWNERS = ["@j0ta29"]


OptolinkSensor = optolink_ns.class_(
    "OptolinkSensor", sensor.Sensor, cg.PollingComponent
)
CONFIG_SCHEMA = (
    sensor.SENSOR_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(OptolinkSensor),
            cv.Required(CONF_ADDRESS): cv.hex_uint32_t,
            cv.Required(CONF_BYTES): cv.one_of(1, 2, 4, int=True),
            cv.Optional(CONF_MIN_VALUE): cv.float_,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(SENSOR_BASE_SCHEMA)
)


async def to_code(config):
    component = await cg.get_variable(config[CONF_OPTOLINK_ID])
    var = cg.new_Pvariable(config[CONF_ID], component)

    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_bytes(config[CONF_BYTES]))
    cg.add(var.set_div_ratio(config[CONF_DIV_RATIO]))
    if CONF_MIN_VALUE in config:
        cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
