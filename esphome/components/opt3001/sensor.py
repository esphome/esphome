import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@ccutrer"]

opt3001_ns = cg.esphome_ns.namespace("opt3001")

OPT3001Sensor = opt3001_ns.class_(
    "OPT3001Sensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(OPT3001Sensor, kind=sensor.ILLUMINANCE, accuracy_decimals=1)
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x44))
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
