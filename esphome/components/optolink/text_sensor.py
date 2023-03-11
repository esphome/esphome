from esphome import core
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_ADDRESS,
    CONF_BYTES,
    CONF_DIV_RATIO,
    CONF_ID,
    CONF_RAW,
    CONF_UPDATE_INTERVAL,
)
from . import optolink_ns, OptolinkComponent, CONF_OPTOLINK_ID
from .sensor import SENSOR_BASE_SCHEMA

OptolinkTextSensor = optolink_ns.class_(
    "OptolinkTextSensor", text_sensor.TextSensor, cg.PollingComponent
)

CONFIG_SCHEMA = cv.All(
    text_sensor.text_sensor_schema(OptolinkTextSensor)
    .extend(
        {
            cv.GenerateID(CONF_OPTOLINK_ID): cv.use_id(OptolinkComponent),
            cv.Optional(CONF_UPDATE_INTERVAL, default="10s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=core.TimePeriod(seconds=1), max=core.TimePeriod(seconds=1800)
                ),
            ),
            cv.Optional(CONF_RAW, default=False): cv.boolean,
        }
    )
    .extend(SENSOR_BASE_SCHEMA)
    .extend({cv.Required(CONF_BYTES): cv.int_}),
)


async def to_code(config):
    component = await cg.get_variable(config[CONF_OPTOLINK_ID])
    var = cg.new_Pvariable(config[CONF_ID], component)

    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)

    cg.add(var.set_raw(config[CONF_RAW]))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_bytes(config[CONF_BYTES]))
    cg.add(var.set_div_ratio(config[CONF_DIV_RATIO]))
