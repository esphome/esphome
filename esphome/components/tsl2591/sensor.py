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
    CONF_ACTUAL_GAIN,
    CONF_ID,
    CONF_NAME,
    CONF_INTEGRATION_TIME,
    CONF_FULL_SPECTRUM,
    CONF_INFRARED,
    CONF_POWER_SAVE_MODE,
    CONF_VISIBLE,
    CONF_CALCULATED_LUX,
    CONF_DEVICE_FACTOR,
    CONF_GLASS_ATTENUATION_FACTOR,
    ICON_BRIGHTNESS_6,
    DEVICE_CLASS_ILLUMINANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
)

DEPENDENCIES = ["i2c"]

tsl2591_ns = cg.esphome_ns.namespace("tsl2591")

TSL2591IntegrationTime = tsl2591_ns.enum("TSL2591IntegrationTime")
INTEGRATION_TIMES = {
    100: TSL2591IntegrationTime.TSL2591_INTEGRATION_TIME_100MS,
    200: TSL2591IntegrationTime.TSL2591_INTEGRATION_TIME_200MS,
    300: TSL2591IntegrationTime.TSL2591_INTEGRATION_TIME_300MS,
    400: TSL2591IntegrationTime.TSL2591_INTEGRATION_TIME_400MS,
    500: TSL2591IntegrationTime.TSL2591_INTEGRATION_TIME_500MS,
    600: TSL2591IntegrationTime.TSL2591_INTEGRATION_TIME_600MS,
}

TSL2591ComponentGain = tsl2591_ns.enum("TSL2591ComponentGain")
GAINS = {
    "1X": TSL2591ComponentGain.TSL2591_CGAIN_LOW,
    "LOW": TSL2591ComponentGain.TSL2591_CGAIN_LOW,
    "25X": TSL2591ComponentGain.TSL2591_CGAIN_MED,
    "MED": TSL2591ComponentGain.TSL2591_CGAIN_MED,
    "MEDIUM": TSL2591ComponentGain.TSL2591_CGAIN_MED,
    "400X": TSL2591ComponentGain.TSL2591_CGAIN_HIGH,
    "HIGH": TSL2591ComponentGain.TSL2591_CGAIN_HIGH,
    "9500X": TSL2591ComponentGain.TSL2591_CGAIN_MAX,
    "MAX": TSL2591ComponentGain.TSL2591_CGAIN_MAX,
    "MAXIMUM": TSL2591ComponentGain.TSL2591_CGAIN_MAX,
    "AUTO": TSL2591ComponentGain.TSL2591_CGAIN_AUTO,
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
                icon=ICON_BRIGHTNESS_6,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VISIBLE): sensor.sensor_schema(
                icon=ICON_BRIGHTNESS_6,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_FULL_SPECTRUM): sensor.sensor_schema(
                icon=ICON_BRIGHTNESS_6,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CALCULATED_LUX): sensor.sensor_schema(
                unit_of_measurement=UNIT_LUX,
                icon=ICON_BRIGHTNESS_6,
                accuracy_decimals=4,
                device_class=DEVICE_CLASS_ILLUMINANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ACTUAL_GAIN): sensor.sensor_schema(
                icon=ICON_BRIGHTNESS_6,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ILLUMINANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(
                CONF_INTEGRATION_TIME, default="100ms"
            ): validate_integration_time,
            cv.Optional(CONF_NAME, default="TLS2591"): cv.string,
            cv.Optional(CONF_GAIN, default="AUTO"): cv.enum(GAINS, upper=True),
            cv.Optional(CONF_POWER_SAVE_MODE, default=True): cv.boolean,
            cv.Optional(CONF_DEVICE_FACTOR, default=53.0): cv.float_with_unit(
                "device_factor", "", True
            ),
            cv.Optional(CONF_GLASS_ATTENUATION_FACTOR, default=7.7): cv.float_with_unit(
                "glass_attenuation_factor", "", True
            ),
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

    if CONF_ACTUAL_GAIN in config:
        conf = config[CONF_ACTUAL_GAIN]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_actual_gain_sensor(sens))

    cg.add(var.set_name(config[CONF_NAME]))
    cg.add(var.set_power_save_mode(config[CONF_POWER_SAVE_MODE]))
    cg.add(var.set_integration_time(config[CONF_INTEGRATION_TIME]))
    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(
        var.set_device_and_glass_attenuation_factors(
            config[CONF_DEVICE_FACTOR], config[CONF_GLASS_ATTENUATION_FACTOR]
        )
    )
