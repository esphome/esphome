# Credit where due....
# I put a certain amount of work into this, but a lot of ESPHome integration is
# "look for other examples and see what they do" programming-by-example. Here are
# things that helped me along with this:
#
# - I mined the existing tsl2591 integration for basic structural framing for both
#   the code and documentation.
#
# - I looked at the existing bme280 integration as an example of a single device
#   with multiple sensors.
#
# - Comments and code in this thread got me going with the Adafruit mlx90614 library
#   and prompted my desired to have mlx90614 as a standard component instead of a
#   custom/external component.
#
# - And, of course, the handy and available Adafruit mlx90614 library was very
#   helpful in understanding what the device is actually talking about.

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_TARGET_TEMPERATURE,
    CONF_REFERENCE_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

DEPENDENCIES = ["i2c"]

mlx90614_ns = cg.esphome_ns.namespace("mlx90614")
MLX90614Component = mlx90614_ns.class_(
    "MLX90614Component", cg.PollingComponent, i2c.I2CDevice
)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MLX90614Component),
            cv.Optional(CONF_TARGET_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_REFERENCE_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("20s"))
    .extend(i2c.i2c_device_schema(0x5A))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_TARGET_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TARGET_TEMPERATURE])
        cg.add(var.set_target_temperature(sens))

    if CONF_REFERENCE_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_REFERENCE_TEMPERATURE])
        cg.add(var.set_reference_temperature(sens))
