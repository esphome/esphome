# Credit where due....
# I put a certain amount of work into this, but a lot of ESPHome integration is
# "look for other examples and see what they do" programming-by-example. Here are
# things that helped me along with this:
#
# - I mined the existing tsl2561 integration for basic structural framing for both
#   the code and documentation.
#
# - I looked at the existing bme280 integration as an example of a single device
#   with multiple sensors.
#
# - Comments and code in this thread got me going with the Adafruit TSL2591 library
#   and prompted my desired to have tsl2591 as a standard component instead of a
#   custom/external component.
#
# - And, of course, the handy and available Adafruit TSL2591 library was very
#   helpful in understanding what the device is actually talking about.
#
# Here is the project that started me down the TSL2591 device trail in the first
# place: https://hackaday.io/project/176690-the-water-watcher

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_GAIN,
    CONF_ID,
    CONF_INTEGRATION_TIME,
    CONF_FULL_SPECTRUM,
    CONF_INFRARED,
    CONF_VISIBLE,
    CONF_CALCULATED_LUX,
    DEVICE_CLASS_ILLUMINANCE,
    STATE_CLASS_MEASUREMENT,
    ICON_LIGHTBULB,
    UNIT_LUX,
)

# The Adafruit sensors library requires both i2c and spi in ESPhome configs, but
# we don't use spi for this component.
DEPENDENCIES = ["i2c", "spi"]

tsl2591_ns = cg.esphome_ns.namespace("tsl2591")

# This enum is a clone of the enum in the Adafruit library.
# I couldn't work out how to use the Adafruit enum directly in codegen.
TSL2591IntegrationTime = tsl2591_ns.enum("TSL2591IntegrationTime")
INTEGRATION_TIMES = {
    100: TSL2591IntegrationTime.TSL2591_INTEGRATION_TIME_100MS,
    200: TSL2591IntegrationTime.TSL2591_INTEGRATION_TIME_200MS,
    300: TSL2591IntegrationTime.TSL2591_INTEGRATION_TIME_300MS,
    400: TSL2591IntegrationTime.TSL2591_INTEGRATION_TIME_400MS,
    500: TSL2591IntegrationTime.TSL2591_INTEGRATION_TIME_500MS,
    600: TSL2591IntegrationTime.TSL2591_INTEGRATION_TIME_600MS,
}

# This enum is a clone of the enum in the Adafruit library.
# I couldn't work out how to use the Adafruit enum directly in codegen.
TSL2591Gain = tsl2591_ns.enum("TSL2591Gain")
GAINS = {
    "1X": TSL2591Gain.TSL2591_GAIN_MULTIPLIER_LOW,
    "LOW": TSL2591Gain.TSL2591_GAIN_MULTIPLIER_LOW,

    "25X": TSL2591Gain.TSL2591_GAIN_MULTIPLIER_MED,
    "MED": TSL2591Gain.TSL2591_GAIN_MULTIPLIER_MED,
    "MEDIUM": TSL2591Gain.TSL2591_GAIN_MULTIPLIER_MED,

    "428X": TSL2591Gain.TSL2591_GAIN_MULTIPLIER_HIGH,
    "HIGH": TSL2591Gain.TSL2591_GAIN_MULTIPLIER_HIGH,

    "9876X": TSL2591Gain.TSL2591_GAIN_MULTIPLIER_MAX,
    "MAX": TSL2591Gain.TSL2591_GAIN_MULTIPLIER_MAX,
    "MAXIMUM": TSL2591Gain.TSL2591_GAIN_MULTIPLIER_MAX,
}


def validate_integration_time(value):
    value = cv.positive_time_period_milliseconds(value).total_milliseconds
    return cv.enum(INTEGRATION_TIMES, int=True)(value)


TSL2591Component = tsl2591_ns.class_(
    "TSL2591Component", cg.PollingComponent, i2c.I2CDevice
)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TSL2591Component),
            cv.Optional(CONF_INFRARED): sensor.sensor_schema(
                UNIT_LUX,
                ICON_LIGHTBULB,
                0,
                DEVICE_CLASS_ILLUMINANCE,
                STATE_CLASS_MEASUREMENT
            ),
            cv.Optional(CONF_VISIBLE): sensor.sensor_schema(
                UNIT_LUX,
                ICON_LIGHTBULB,
                0,
                DEVICE_CLASS_ILLUMINANCE,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_FULL_SPECTRUM): sensor.sensor_schema(
                UNIT_LUX,
                ICON_LIGHTBULB,
                0,
                DEVICE_CLASS_ILLUMINANCE,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CALCULATED_LUX): sensor.sensor_schema(
                UNIT_LUX,
                ICON_LIGHTBULB,
                4,
                DEVICE_CLASS_ILLUMINANCE,
                STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_INTEGRATION_TIME, default="100ms"): validate_integration_time,
            cv.Optional(CONF_GAIN, default="MEDIUM"): cv.enum(GAINS, upper=True),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x29))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_FULL_SPECTRUM in config:
        conf = config[CONF_FULL_SPECTRUM]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_full_spectrum_sensor(sens))

    if CONF_INFRARED in config:
        conf = config[CONF_INFRARED]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_infrared_sensor(sens))

    if CONF_VISIBLE in config:
        conf = config[CONF_VISIBLE]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_visible_sensor(sens))

    if CONF_CALCULATED_LUX in config:
        conf = config[CONF_CALCULATED_LUX]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_calculated_lux_sensor(sens))

    cg.add(var.set_integration_time(config[CONF_INTEGRATION_TIME]))
    cg.add(var.set_gain(config[CONF_GAIN]))
    # https://platformio.org/lib/show/463/Adafruit%20TSL2591%20Library
    cg.add_library("Adafruit TSL2591 Library", "^1.4.0")
