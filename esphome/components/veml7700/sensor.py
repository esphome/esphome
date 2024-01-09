import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_GAIN,
    CONF_GLASS_ATTENUATION_FACTOR,
    CONF_ACTUAL_GAIN,
    CONF_AUTO_MODE,
    CONF_FULL_SPECTRUM,
    CONF_INTEGRATION_TIME,
    UNIT_LUX,
    UNIT_MILLISECOND,
    ICON_BRIGHTNESS_5,
    ICON_TIMER,
    DEVICE_CLASS_ILLUMINANCE,
    STATE_CLASS_MEASUREMENT,
)

CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["i2c"]

UNIT_COUNTS = "#"
ICON_GAIN = "mdi:multiplication"

CONF_ACTUAL_INTEGRATION_TIME = "actual_integration_time"
CONF_AMBIENT_LIGHT = "ambient_light"
CONF_AMBIENT_LIGHT_COUNTS = "ambient_light_counts"
CONF_FULL_SPECTRUM_COUNTS = "full_spectrum_counts"
CONF_LUX_COMPENSATION = "lux_compensation"

veml7700_ns = cg.esphome_ns.namespace("veml7700")

VEML7700Component = veml7700_ns.class_(
    "VEML7700Component", cg.PollingComponent, i2c.I2CDevice
)

Gain = veml7700_ns.enum("Gain")
GAINS = {
    "1/8x": Gain.X_1_8,
    "1/4x": Gain.X_1_4,
    "1x": Gain.X_1,
    "2x": Gain.X_2,
}

IntegrationTime = veml7700_ns.enum("IntegrationTime")
INTEGRATION_TIMES = {
    25: IntegrationTime.INTEGRATION_TIME_25MS,
    50: IntegrationTime.INTEGRATION_TIME_50MS,
    100: IntegrationTime.INTEGRATION_TIME_100MS,
    200: IntegrationTime.INTEGRATION_TIME_200MS,
    400: IntegrationTime.INTEGRATION_TIME_400MS,
    800: IntegrationTime.INTEGRATION_TIME_800MS,
}


def validate_integration_time(value):
    value = cv.positive_time_period_milliseconds(value).total_milliseconds
    return cv.enum(INTEGRATION_TIMES, int=True)(value)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VEML7700Component),
            cv.Optional(CONF_AUTO_MODE, default=True): cv.boolean,
            cv.Optional(CONF_LUX_COMPENSATION, default=True): cv.boolean,
            cv.Optional(CONF_AMBIENT_LIGHT): sensor.sensor_schema(
                unit_of_measurement=UNIT_LUX,
                icon=ICON_BRIGHTNESS_5,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ILLUMINANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AMBIENT_LIGHT_COUNTS): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS,
                icon=ICON_BRIGHTNESS_5,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ILLUMINANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_FULL_SPECTRUM): sensor.sensor_schema(
                unit_of_measurement=UNIT_LUX,
                icon=ICON_BRIGHTNESS_5,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ILLUMINANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_FULL_SPECTRUM_COUNTS): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS,
                icon=ICON_BRIGHTNESS_5,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ILLUMINANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_GAIN, default="1x"): cv.enum(GAINS),
            cv.Optional(CONF_ACTUAL_GAIN): sensor.sensor_schema(
                icon=ICON_GAIN,
                accuracy_decimals=3,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(
                CONF_INTEGRATION_TIME, default="100ms"
            ): validate_integration_time,
            cv.Optional(CONF_ACTUAL_INTEGRATION_TIME): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLISECOND,
                icon=ICON_TIMER,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_GLASS_ATTENUATION_FACTOR, default=1.0): cv.float_range(
                min=1.0
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x10)),
    cv.has_at_least_one_key(
        CONF_AMBIENT_LIGHT,
        CONF_FULL_SPECTRUM,
        CONF_AMBIENT_LIGHT_COUNTS,
        CONF_FULL_SPECTRUM_COUNTS,
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_AMBIENT_LIGHT in config:
        conf = config[CONF_AMBIENT_LIGHT]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_ambient_light_sensor(sens))

    if CONF_AMBIENT_LIGHT_COUNTS in config:
        conf = config[CONF_AMBIENT_LIGHT_COUNTS]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_ambient_light_counts_sensor(sens))

    if CONF_FULL_SPECTRUM in config:
        conf = config[CONF_FULL_SPECTRUM]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_white_sensor(sens))

    if CONF_FULL_SPECTRUM_COUNTS in config:
        conf = config[CONF_FULL_SPECTRUM_COUNTS]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_white_counts_sensor(sens))

    if CONF_ACTUAL_GAIN in config:
        conf = config[CONF_ACTUAL_GAIN]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_actual_gain_sensor(sens))

    if CONF_ACTUAL_INTEGRATION_TIME in config:
        conf = config[CONF_ACTUAL_INTEGRATION_TIME]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_actual_integration_time_sensor(sens))

    cg.add(var.set_enable_automatic_mode(config[CONF_AUTO_MODE]))
    cg.add(var.set_enable_lux_compensation(config[CONF_LUX_COMPENSATION]))
    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_integration_time(config[CONF_INTEGRATION_TIME]))
    cg.add(var.set_attenuation_factor(config[CONF_GLASS_ATTENUATION_FACTOR]))
