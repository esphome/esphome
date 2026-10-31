import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]

sts3x_ns = cg.esphome_ns.namespace("sts3x")

STS3XComponent = sts3x_ns.class_(
    "STS3XComponent", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(STS3XComponent, kind=sensor.TEMPERATURE, accuracy_decimals=1)
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x4A))
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
