import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CALIBRATION,
    CONF_CLEAR,
    CONF_COLOR_TEMPERATURE,
    CONF_GAIN,
    CONF_GLASS_ATTENUATION_FACTOR,
    CONF_ID,
    CONF_ILLUMINANCE,
    CONF_NAME,
    DEVICE_CLASS_ILLUMINANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_KELVIN,
    UNIT_LUX,
    UNIT_PERCENT,
)

CODEOWNERS = ["@mrgnr", "@latonita"]
DEPENDENCIES = ["i2c"]

as7341_ns = cg.esphome_ns.namespace("as7341")

AS7341Component = as7341_ns.class_(
    "AS7341Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_ATIME = "atime"
CONF_ASTEP = "astep"
CONF_DARK_CURRENT = "dark_current"
CONF_CHANNEL_CORRECTION = "channel_correction"

CONF_F1 = "f1"
CONF_F2 = "f2"
CONF_F3 = "f3"
CONF_F4 = "f4"
CONF_F5 = "f5"
CONF_F6 = "f6"
CONF_F7 = "f7"
CONF_F8 = "f8"
CONF_NIR = "nir"

CONF_BAND_COUNTS = "band_counts"
CONF_BAND_BASIC_COUNTS = "band_basic_counts"

CONF_IRRADIANCE = "irradiance"
CONF_IRRADIANCE_PHOTOPIC = "irradiance_photopic"
CONF_PPFD = "ppfd"
CONF_IRRADIANCE_PAR = "irradiance_par"
CONF_SATURATION_LEVEL = "saturation_level"

UNIT_COUNTS = "#"
UNIT_IRRADIANCE = "W/m²"
UNIT_PPFD = "µmol/s⋅m²"

ICON_BAND_COUNTS = "mdi:counter"
ICON_IRRADIANCE = "mdi:radioactive"
ICON_ILLUMINANCE = "mdi:weather-sunny"
ICON_IRRADIANCE_PHOTOPIC = "mdi:sun-wireless-outline"
ICON_PAR = "mdi:sprout"
ICON_PPFD = "mdi:sprout-outline"
ICON_COLOR_TEMPERATURE = "mdi:sun-thermometer-outline"
ICON_SATURATION = "mdi:weather-sunny-alert"

AS7341_GAIN = as7341_ns.enum("AS7341Gain")
GAIN_OPTIONS = {
    "X0.5": AS7341_GAIN.AS7341_GAIN_0_5X,
    "X1": AS7341_GAIN.AS7341_GAIN_1X,
    "X2": AS7341_GAIN.AS7341_GAIN_2X,
    "X4": AS7341_GAIN.AS7341_GAIN_4X,
    "X8": AS7341_GAIN.AS7341_GAIN_8X,
    "X16": AS7341_GAIN.AS7341_GAIN_16X,
    "X32": AS7341_GAIN.AS7341_GAIN_32X,
    "X64": AS7341_GAIN.AS7341_GAIN_64X,
    "X128": AS7341_GAIN.AS7341_GAIN_128X,
    "X256": AS7341_GAIN.AS7341_GAIN_256X,
    "X512": AS7341_GAIN.AS7341_GAIN_512X,
}


SENSOR_SCHEMA = cv.maybe_simple_value(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_COUNTS,
        icon=ICON_BAND_COUNTS,
        accuracy_decimals=6,
        device_class=DEVICE_CLASS_ILLUMINANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    key=CONF_NAME,
)

FLOAT_10_SCHEMA = cv.Schema(
    cv.All(
        cv.ensure_list(cv.float_),
        cv.Length(min=10, max=10),
    ),
)

CALIBRATION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_CHANNEL_CORRECTION, default=[1.0] * 10): FLOAT_10_SCHEMA,
        cv.Optional(CONF_DARK_CURRENT, default=[0.0] * 10): FLOAT_10_SCHEMA,
        cv.Optional(CONF_GLASS_ATTENUATION_FACTOR, default=1.0): cv.float_,
    }
)

COUNTS_SENSOR_SCHEMA = cv.maybe_simple_value(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_COUNTS,
        icon=ICON_BAND_COUNTS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_ILLUMINANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    key=CONF_NAME,
)

BASIC_COUNTS_SENSOR_SCHEMA = cv.maybe_simple_value(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_COUNTS,
        icon=ICON_BAND_COUNTS,
        accuracy_decimals=6,
        device_class=DEVICE_CLASS_ILLUMINANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    key=CONF_NAME,
)

BAND_COUNTS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_F1): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F2): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F3): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F4): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F5): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F6): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F7): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F8): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_NIR): COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_CLEAR): COUNTS_SENSOR_SCHEMA,
    }
)

BAND_BASIC_COUNTS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_F1): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F2): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F3): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F4): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F5): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F6): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F7): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_F8): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_NIR): BASIC_COUNTS_SENSOR_SCHEMA,
        cv.Optional(CONF_CLEAR): BASIC_COUNTS_SENSOR_SCHEMA,
    }
)
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AS7341Component),
            cv.Optional(CONF_GAIN, default="X8"): cv.enum(GAIN_OPTIONS),
            cv.Optional(CONF_ATIME, default=29): cv.int_range(min=0, max=255),
            cv.Optional(CONF_ASTEP, default=599): cv.int_range(min=0, max=65534),
            cv.Optional(CONF_CALIBRATION): CALIBRATION_SCHEMA,
            cv.Optional(CONF_BAND_COUNTS): BAND_COUNTS_SCHEMA,
            cv.Optional(CONF_BAND_BASIC_COUNTS): BAND_BASIC_COUNTS_SCHEMA,
            cv.Optional(CONF_ILLUMINANCE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_LUX,
                    icon=ICON_ILLUMINANCE,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_IRRADIANCE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_IRRADIANCE,
                    icon=ICON_IRRADIANCE,
                    accuracy_decimals=3,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_IRRADIANCE_PHOTOPIC): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_IRRADIANCE,
                    icon=ICON_IRRADIANCE_PHOTOPIC,
                    accuracy_decimals=3,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_PPFD): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_PPFD,
                    icon=ICON_PPFD,
                    accuracy_decimals=3,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_IRRADIANCE_PAR): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_IRRADIANCE,
                    icon=ICON_PAR,
                    accuracy_decimals=3,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_COLOR_TEMPERATURE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_KELVIN,
                    icon=ICON_COLOR_TEMPERATURE,
                    accuracy_decimals=0,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_SATURATION_LEVEL): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_PERCENT,
                    icon=ICON_SATURATION,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x39))
)


BANDS = {
    CONF_F1: 0,
    CONF_F2: 1,
    CONF_F3: 2,
    CONF_F4: 3,
    CONF_F5: 6,
    CONF_F6: 7,
    CONF_F7: 8,
    CONF_F8: 9,
    CONF_NIR: 10,
    CONF_CLEAR: 11,
}


SENSORS_INTEGRAL = [
    CONF_ILLUMINANCE,
    CONF_IRRADIANCE,
    CONF_IRRADIANCE_PHOTOPIC,
    CONF_IRRADIANCE_PAR,
    CONF_PPFD,
    CONF_COLOR_TEMPERATURE,
    CONF_SATURATION_LEVEL,
]


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_atime(config[CONF_ATIME]))
    cg.add(var.set_astep(config[CONF_ASTEP]))

    if calibration := config.get(CONF_CALIBRATION):
        cg.add(var.set_dark_current_calibration(calibration[CONF_DARK_CURRENT]))
        cg.add(var.set_channel_correction(calibration[CONF_CHANNEL_CORRECTION]))
        cg.add(
            var.set_glass_attenuation_factor(calibration[CONF_GLASS_ATTENUATION_FACTOR])
        )

    if counts_config := config.get(CONF_BAND_COUNTS):
        for key, ch in BANDS.items():
            if sensor_config := counts_config.get(key):
                sens = await sensor.new_sensor(sensor_config)
                cg.add(var.set_band_counts_sensor(sens, ch))

    if basic_counts_config := config.get(CONF_BAND_BASIC_COUNTS):
        for key, ch in BANDS.items():
            if sensor_config := basic_counts_config.get(key):
                sens = await sensor.new_sensor(sensor_config)
                cg.add(var.set_band_basic_counts_sensor(sens, ch))

    for sensor_id in SENSORS_INTEGRAL:
        if sensor_config := config.get(sensor_id):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(getattr(var, f"set_{sensor_id}_sensor")(sens))
