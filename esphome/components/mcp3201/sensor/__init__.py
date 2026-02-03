import esphome.codegen as cg
from esphome.components import sensor, voltage_sampler
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import MCP3201, mcp3201_ns

AUTO_LOAD = ["voltage_sampler"]

DEPENDENCIES = ["mcp3201"]

MCP3201Sensor = mcp3201_ns.class_(
    "MCP3201Sensor", sensor.Sensor, cg.PollingComponent, voltage_sampler.VoltageSampler
)
CONF_MCP3201_ID = "mcp3201_id"

CONFIG_SCHEMA = (
    sensor.sensor_schema(MCP3201Sensor)
    .extend(
        {
            cv.GenerateID(CONF_MCP3201_ID): cv.use_id(MCP3201),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_MCP3201_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
