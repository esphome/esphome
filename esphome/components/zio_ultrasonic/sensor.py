import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_DISTANCE, STATE_CLASS_MEASUREMENT

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@kahrendt"]

zio_ultrasonic_ns = cg.esphome_ns.namespace("zio_ultrasonic")

ZioUltrasonicComponent = zio_ultrasonic_ns.class_(
    "ZioUltrasonicComponent", cg.PollingComponent, i2c.I2CDevice, sensor.Sensor
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        ZioUltrasonicComponent,
        unit_of_measurement="mm",
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_DISTANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x00))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
