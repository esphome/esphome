from esphome import core
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_ADDRESS,
    CONF_BYTES,
    CONF_DIV_RATIO,
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
    CONF_UPDATE_INTERVAL,
)
from .sensor import SENSOR_BASE_SCHEMA
from . import OptolinkComponent, optolink_ns, CONF_OPTOLINK_ID

OptolinkNumber = optolink_ns.class_(
    "OptolinkNumber", number.Number, cg.PollingComponent
)

CONFIG_SCHEMA = (
    number.NUMBER_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OPTOLINK_ID): cv.use_id(OptolinkComponent),
            cv.GenerateID(): cv.declare_id(OptolinkNumber),
            cv.Required(CONF_MAX_VALUE): cv.float_,
            cv.Required(CONF_MIN_VALUE): cv.float_range(min=0.0),
            cv.Required(CONF_STEP): cv.float_,
            cv.Optional(CONF_UPDATE_INTERVAL, default="10s"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(
                    min=core.TimePeriod(seconds=1), max=core.TimePeriod(seconds=1800)
                ),
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(SENSOR_BASE_SCHEMA)
)


async def to_code(config):
    component = await cg.get_variable(config[CONF_OPTOLINK_ID])
    var = cg.new_Pvariable(config[CONF_ID], component)

    await cg.register_component(var, config)
    await number.register_number(
        var,
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )

    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_bytes(config[CONF_BYTES]))
    cg.add(var.set_div_ratio(config[CONF_DIV_RATIO]))
