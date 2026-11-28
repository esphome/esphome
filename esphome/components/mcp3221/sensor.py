import esphome.codegen as cg
from esphome.components import i2c, sensor, voltage_sampler
import esphome.config_validation as cv
from esphome.const import CONF_REFERENCE_VOLTAGE
from esphome.types import ConfigType

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
    sensor.sensor_schema(MCP3221Sensor, kind=sensor.VOLTAGE, accuracy_decimals=2)
    .extend(
        {
            cv.Optional(CONF_REFERENCE_VOLTAGE, default="3.3V"): cv.voltage,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x48))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    await sensor.register_sensor(var, config)
    cg.add(var.set_reference_voltage(config[CONF_REFERENCE_VOLTAGE]))
