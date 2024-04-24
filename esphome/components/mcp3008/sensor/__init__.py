import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, voltage_sampler
from esphome.const import (
    CONF_ID,
    CONF_NUMBER,
    UNIT_VOLT,
    STATE_CLASS_MEASUREMENT,
    DEVICE_CLASS_VOLTAGE,
)

from .. import mcp3008_ns, MCP3008

AUTO_LOAD = ["voltage_sampler"]

DEPENDENCIES = ["mcp3008"]

MCP3008Sensor = mcp3008_ns.class_(
    "MCP3008Sensor",
    sensor.Sensor,
    cg.PollingComponent,
    voltage_sampler.VoltageSampler,
    cg.Parented.template(MCP3008),
)
CONF_REFERENCE_VOLTAGE = "reference_voltage"
CONF_MCP3008_ID = "mcp3008_id"

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        MCP3008Sensor,
        unit_of_measurement=UNIT_VOLT,
        state_class=STATE_CLASS_MEASUREMENT,
        device_class=DEVICE_CLASS_VOLTAGE,
    )
    .extend(
        {
            cv.GenerateID(CONF_MCP3008_ID): cv.use_id(MCP3008),
            cv.Required(CONF_NUMBER): cv.int_,
            cv.Optional(CONF_REFERENCE_VOLTAGE, default="3.3V"): cv.voltage,
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_MCP3008_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_reference_voltage(config[CONF_REFERENCE_VOLTAGE]))
