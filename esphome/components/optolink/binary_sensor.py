from esphome import core
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID, CONF_ADDRESS, CONF_UPDATE_INTERVAL
from . import OptolinkComponent, optolink_ns, CONF_OPTOLINK_ID

OptolinkBinarySensor = optolink_ns.class_(
    "OptolinkBinarySensor", binary_sensor.BinarySensor, cg.PollingComponent
)
CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(OptolinkBinarySensor).extend(
    {
        cv.GenerateID(CONF_OPTOLINK_ID): cv.use_id(OptolinkComponent),
        cv.Required(CONF_ADDRESS): cv.hex_uint32_t,
        cv.Optional(CONF_UPDATE_INTERVAL, default="10s"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=core.TimePeriod(seconds=1), max=core.TimePeriod(seconds=1800)),
        ),
    }
)


async def to_code(config):
    component = await cg.get_variable(config[CONF_OPTOLINK_ID])
    var = cg.new_Pvariable(config[CONF_ID], component)

    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)

    cg.add(var.set_address(config[CONF_ADDRESS]))
