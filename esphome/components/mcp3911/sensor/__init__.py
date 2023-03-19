import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, voltage_sampler
from esphome.const import CONF_ID, CONF_CHANNEL

from .. import mcp3911_ns, MCP3911

AUTO_LOAD = ["voltage_sampler"]
DEPENDENCIES = ["mcp3911"]

MCP3911Sensor = mcp3911_ns.class_(
    "MCP3911Sensor",
    sensor.Sensor,
    cg.PollingComponent,
    voltage_sampler.VoltageSampler,
)
CONF_MCP3911_ID = "mcp3911_id"

CONFIG_SCHEMA = sensor.SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(MCP3911Sensor),
        cv.GenerateID(CONF_MCP3911_ID): cv.use_id(MCP3911),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=1),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_CHANNEL],
    )
    await cg.register_parented(var, config[CONF_MCP3911_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)