from esphome import core
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_ADDRESS,
    CONF_BYTES,
    CONF_DIV_RATIO,
    CONF_UPDATE_INTERVAL,
)
from . import optolink_ns, OptolinkComponent

OptolinkSensor = optolink_ns.class_(
    "OptolinkSensor", sensor.Sensor, cg.PollingComponent
)
CONF_OPTOLINK_ID = "optolink_id"
SENSOR_BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ADDRESS): cv.hex_uint32_t,
        cv.Required(CONF_BYTES): cv.one_of(1, 2, 4, int=True),
        cv.Optional(CONF_DIV_RATIO, default=1): cv.one_of(1, 10, 100, 3600, int=True),
    }
)
CONFIG_SCHEMA = (
    sensor.sensor_schema(OptolinkSensor)
    .extend(
        {
            cv.GenerateID(CONF_OPTOLINK_ID): cv.use_id(OptolinkComponent),
            cv.Optional(CONF_UPDATE_INTERVAL, default="10s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=core.TimePeriod(seconds=1), max=core.TimePeriod(seconds=1800)
                ),
            ),
        }
    )
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
