import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_RESOLUTION,
    DEVICE_CLASS_ILLUMINANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
    CONF_MEASUREMENT_DURATION,
)

DEPENDENCIES = ["i2c"]

bh1750_ns = cg.esphome_ns.namespace("bh1750")
BH1750Resolution = bh1750_ns.enum("BH1750Resolution")
BH1750_RESOLUTIONS = {
    4.0: BH1750Resolution.BH1750_RESOLUTION_4P0_LX,
    1.0: BH1750Resolution.BH1750_RESOLUTION_1P0_LX,
    0.5: BH1750Resolution.BH1750_RESOLUTION_0P5_LX,
}

BH1750Sensor = bh1750_ns.class_(
    "BH1750Sensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

CONF_MEASUREMENT_TIME = "measurement_time"
CONFIG_SCHEMA = (
    sensor.sensor_schema(
        unit_of_measurement=UNIT_LUX,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_ILLUMINANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(BH1750Sensor),
            cv.Optional(CONF_RESOLUTION, default=0.5): cv.enum(
                BH1750_RESOLUTIONS, float=True
            ),
            cv.Optional(CONF_MEASUREMENT_DURATION, default=69): cv.int_range(
                min=31, max=254
            ),
            cv.Optional(CONF_MEASUREMENT_TIME): cv.invalid(
                "The 'measurement_time' option has been replaced with 'measurement_duration' in 1.18.0"
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x23))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_resolution(config[CONF_RESOLUTION]))
    cg.add(var.set_measurement_duration(config[CONF_MEASUREMENT_DURATION]))
