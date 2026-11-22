import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@B48D81EFCC"]

sensor_ns = cg.esphome_ns.namespace("bh1900nux")
BH1900NUXSensor = sensor_ns.class_(
    "BH1900NUXSensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(BH1900NUXSensor, kind=sensor.TEMPERATURE)
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x48))
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
