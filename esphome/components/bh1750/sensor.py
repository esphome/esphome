import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    DEVICE_CLASS_ILLUMINANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
)

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@OttoWinter"]

bh1750_ns = cg.esphome_ns.namespace("bh1750")

BH1750Sensor = bh1750_ns.class_(
    "BH1750Sensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        BH1750Sensor,
        unit_of_measurement=UNIT_LUX,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_ILLUMINANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional("resolution"): cv.invalid(
                "The 'resolution' option has been removed. The optimal value is now dynamically calculated."
            ),
            cv.Optional("measurement_duration"): cv.invalid(
                "The 'measurement_duration' option has been removed. The optimal value is now dynamically calculated."
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x23))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
