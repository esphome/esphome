import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import ICON_MOTION_SENSOR

CODEOWNERS = ["@shreyaskarnik"]
DEPENDENCIES = ["i2c"]

sen21231_sensor_ns = cg.esphome_ns.namespace("sen21231_sensor")
Sen21231Sensor = sen21231_sensor_ns.class_(
    "Sen21231Sensor", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(Sen21231Sensor, icon=ICON_MOTION_SENSOR, accuracy_decimals=1)
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x62))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
