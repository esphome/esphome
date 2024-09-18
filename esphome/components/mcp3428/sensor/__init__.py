import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, voltage_sampler
from esphome.const import (
    CONF_GAIN,
    CONF_MULTIPLEXER,
    CONF_RESOLUTION,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
)
from .. import mcp3428_ns, MCP3428Component, CONF_MCP3428_ID

CODEOWNERS = ["@mdop"]
AUTO_LOAD = ["voltage_sampler"]
DEPENDENCIES = ["mcp3428"]

MCP3428Multiplexer = mcp3428_ns.enum("MCP3428Multiplexer")
MUX = {
    1: MCP3428Multiplexer.MCP3428_MULTIPLEXER_CHANNEL_1,
    2: MCP3428Multiplexer.MCP3428_MULTIPLEXER_CHANNEL_2,
    3: MCP3428Multiplexer.MCP3428_MULTIPLEXER_CHANNEL_3,
    4: MCP3428Multiplexer.MCP3428_MULTIPLEXER_CHANNEL_4,
}

MCP3428Gain = mcp3428_ns.enum("MCP3428Gain")
GAIN = {
    1: MCP3428Gain.MCP3428_GAIN_1,
    2: MCP3428Gain.MCP3428_GAIN_2,
    4: MCP3428Gain.MCP3428_GAIN_4,
    8: MCP3428Gain.MCP3428_GAIN_8,
}

MCP3428Resolution = mcp3428_ns.enum("MCP3428Resolution")
RESOLUTION = {
    12: MCP3428Resolution.MCP3428_12_BITS,
    14: MCP3428Resolution.MCP3428_14_BITS,
    16: MCP3428Resolution.MCP3428_16_BITS,
}


MCP3428Sensor = mcp3428_ns.class_(
    "MCP3428Sensor", sensor.Sensor, cg.PollingComponent, voltage_sampler.VoltageSampler
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        MCP3428Sensor,
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=6,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_MCP3428_ID): cv.use_id(MCP3428Component),
            cv.Required(CONF_MULTIPLEXER): cv.enum(MUX, int=True),
            cv.Optional(CONF_GAIN, default=1): cv.enum(GAIN, int=True),
            cv.Optional(CONF_RESOLUTION, default=16): cv.enum(RESOLUTION, int=True),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_MCP3428_ID])

    cg.add(var.set_multiplexer(config[CONF_MULTIPLEXER]))
    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_resolution(config[CONF_RESOLUTION]))
