from esphome import core
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_UPDATE_INTERVAL
from .. import CONF_OPTOLINK_ID, SENSOR_BASE_SCHEMA, optolink_ns

DEPENDENCIES = ["optolink"]
CODEOWNERS = ["@j0ta29"]


OptolinkSwitch = optolink_ns.class_(
    "OptolinkSwitch", switch.Switch, cg.PollingComponent
)

CONFIG_SCHEMA = (
    switch.SWITCH_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(OptolinkSwitch),
            cv.Required(CONF_ADDRESS): cv.hex_uint32_t,
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
    await switch.register_switch(var, config)

    cg.add(var.set_address(config[CONF_ADDRESS]))
