import esphome.codegen as cg
from esphome.components import i2c, sensor, voltage_sampler
import esphome.config_validation as cv
from esphome.const import (
    CONF_REFERENCE_VOLTAGE,
    DEVICE_CLASS_VOLTAGE,
    ICON_SCALE,
    STATE_CLASS_MEASUREMENT,
    UNIT_VOLT,
)

AUTO_LOAD = ["voltage_sampler"]
DEPENDENCIES = ["i2c"]


mcp3221_ns = cg.esphome_ns.namespace("mcp3221")
MCP3221Sensor = mcp3221_ns.class_(
    "MCP3221Sensor",
    sensor.Sensor,
    voltage_sampler.VoltageSampler,
    cg.PollingComponent,
    i2c.I2CDevice,
)


CONFIG_SCHEMA = (
    sensor.sensor_schema(
        MCP3221Sensor,
        icon=ICON_SCALE,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
        device_class=DEVICE_CLASS_VOLTAGE,
        unit_of_measurement=UNIT_VOLT,
    )
    .extend(
        {
            cv.Optional(CONF_REFERENCE_VOLTAGE, default="3.3V"): cv.voltage,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x48))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    cg.add(var.set_reference_voltage(config[CONF_REFERENCE_VOLTAGE]))
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
