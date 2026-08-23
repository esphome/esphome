import esphome.codegen as cg
from esphome.components import i2c, sensor, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CALIBRATION,
    CONF_CLEAR,
    CONF_COLOR_TEMPERATURE,
    CONF_COUNTS,
    CONF_GAIN,
    CONF_GLASS_ATTENUATION_FACTOR,
    CONF_ID,
    CONF_ILLUMINANCE,
    CONF_NAME,
    CONF_TYPE,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_IRRADIANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_KELVIN,
    UNIT_LUX,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

CODEOWNERS = ["@latonita", "@mrgnr"]
AUTO_LOAD = ["text_sensor"]
DEPENDENCIES = ["i2c"]

CONF_ATIME = "atime"
CONF_ASTEP = "astep"
CONF_BASIC_COUNTS = "basic_counts"
CONF_NORMALIZE_BASIC_COUNTS = "normalize_basic_counts"
CONF_CHANNEL_CORRECTION = "channel_correction"
CONF_DARK_CURRENT = "dark_current"
CONF_SATURATION_LEVEL = "saturation_level"
CONF_IRRADIANCE_PHOTOPIC = "irradiance_photopic"
CONF_IRRADIANCE_PAR = "irradiance_par"
CONF_PPFD = "ppfd"
CONF_RGB_HEX = "rgb_hex"

CONF_F1 = "f1"
CONF_F2 = "f2"
CONF_FZ = "fz"
CONF_F3 = "f3"
CONF_F4 = "f4"
CONF_FY = "fy"
CONF_F5 = "f5"
CONF_FXL = "fxl"
CONF_F6 = "f6"
CONF_F7 = "f7"
CONF_F8 = "f8"
CONF_NIR = "nir"

UNIT_COUNTS = "#"
UNIT_IRRADIANCE = "W/m²"
UNIT_PPFD = "µmol/m²/s"

ICON_COUNTS = "mdi:counter"
ICON_SATURATION = "mdi:weather-sunny-alert"
ICON_ILLUMINANCE = "mdi:weather-sunny"
ICON_IRRADIANCE_PHOTOPIC = "mdi:sun-wireless-outline"
ICON_PAR = "mdi:sprout"
ICON_PPFD = "mdi:sprout-outline"
ICON_COLOR_TEMPERATURE = "mdi:sun-thermometer-outline"
ICON_PALETTE = "mdi:palette"

MODEL_AS7341 = "AS7341"
MODEL_AS7343 = "AS7343"
MODEL_TCS3448 = "TCS3448"

as734x_ns = cg.esphome_ns.namespace("as734x")
AS734XComponent = as734x_ns.class_(
    "AS734XComponent", cg.PollingComponent, i2c.I2CDevice
)

Normalization = as734x_ns.enum("Normalization", True)
NORMALIZATION_OPTIONS = {
    "NONE": Normalization.NONE,
    "ALL": Normalization.ALL,
    "BANDS": Normalization.BANDS,
    "CLEAR": Normalization.CLEAR,
}

AS734X_Models = as734x_ns.enum("Model", True)
AS734X_MODELS = {
    MODEL_AS7341: AS734X_Models.AS7341,
    MODEL_AS7343: AS734X_Models.AS7343,
    MODEL_TCS3448: AS734X_Models.TCS3448,
}

# The TCS3448 shares the AS7343 register map and calibration, but answers on a different address.
TCS3448_ADDRESS = 0x59

Gain = as734x_ns.enum("Gain")
GAIN_OPTIONS_41 = {
    "X0.5": Gain.GAIN_0_5X,
    "X1": Gain.GAIN_1X,
    "X2": Gain.GAIN_2X,
    "X4": Gain.GAIN_4X,
    "X8": Gain.GAIN_8X,
    "X16": Gain.GAIN_16X,
    "X32": Gain.GAIN_32X,
    "X64": Gain.GAIN_64X,
    "X128": Gain.GAIN_128X,
    "X256": Gain.GAIN_256X,
    "X512": Gain.GAIN_512X,
}

GAIN_OPTIONS_43 = {
    **GAIN_OPTIONS_41,
    "X1024": Gain.GAIN_1024X,
    "X2048": Gain.GAIN_2048X,
}

COUNTS_SENSOR_SCHEMA = cv.maybe_simple_value(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_COUNTS,
        icon=ICON_COUNTS,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_EMPTY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    key=CONF_NAME,
)

# Basic counts are fractional, so they need decimals where raw counts do not.
BASIC_COUNTS_SENSOR_SCHEMA = cv.maybe_simple_value(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_COUNTS,
        icon=ICON_COUNTS,
        accuracy_decimals=6,
        device_class=DEVICE_CLASS_EMPTY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    key=CONF_NAME,
)

# Channel order is significant: the index is used to set the sensor in C++.
BANDS_41 = (
    CONF_F1,
    CONF_F2,
    CONF_F3,
    CONF_F4,
    CONF_F5,
    CONF_F6,
    CONF_F7,
    CONF_F8,
    CONF_NIR,
    CONF_CLEAR,
)
BANDS_43 = (
    CONF_F1,
    CONF_F2,
    CONF_FZ,
    CONF_F3,
    CONF_F4,
    CONF_FY,
    CONF_F5,
    CONF_FXL,
    CONF_F6,
    CONF_F7,
    CONF_F8,
    CONF_NIR,
    CONF_CLEAR,
)

COUNTS_SCHEMA_41 = cv.Schema(
    {cv.Optional(band): COUNTS_SENSOR_SCHEMA for band in BANDS_41}
)
COUNTS_SCHEMA_43 = cv.Schema(
    {cv.Optional(band): COUNTS_SENSOR_SCHEMA for band in BANDS_43}
)

BASIC_COUNTS_SCHEMA_41 = cv.Schema(
    {cv.Optional(band): BASIC_COUNTS_SENSOR_SCHEMA for band in BANDS_41}
)
BASIC_COUNTS_SCHEMA_43 = cv.Schema(
    {cv.Optional(band): BASIC_COUNTS_SENSOR_SCHEMA for band in BANDS_43}
)


def _float_list(length: int) -> cv.All:
    """One float per channel, so the list length has to match the model."""
    return cv.All(cv.ensure_list(cv.float_), cv.Length(min=length, max=length))


# Per-channel correction for the golden device, measured by the manufacturer.
DEFAULT_CHANNEL_CORRECTION_41 = [1.0] * 10
DEFAULT_CHANNEL_CORRECTION_43 = [
    1.055464349,
    1.043509797,
    1.029576268,
    1.0175052,
    1.00441899,
    0.987356499,
    0.957597044,
    0.995863485,
    1.014628964,
    0.996500814,
    0.933072749,
    1.052236338,
    0.999570232,
]


def _calibration_schema(channels: int, default_correction: list[float]) -> cv.Schema:
    return cv.Schema(
        {
            cv.Optional(
                CONF_CHANNEL_CORRECTION, default=default_correction
            ): _float_list(channels),
            cv.Optional(CONF_DARK_CURRENT, default=[0.0] * channels): _float_list(
                channels
            ),
            cv.Optional(CONF_GLASS_ATTENUATION_FACTOR, default=1.0): cv.positive_float,
        }
    )


CALIBRATION_SCHEMA_41 = _calibration_schema(10, DEFAULT_CHANNEL_CORRECTION_41)
CALIBRATION_SCHEMA_43 = _calibration_schema(13, DEFAULT_CHANNEL_CORRECTION_43)

_COMMON_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AS734XComponent),
            cv.Optional(CONF_ATIME, default=29): cv.int_range(min=0, max=255),
            cv.Optional(CONF_ASTEP, default=599): cv.int_range(min=0, max=65534),
            cv.Optional(CONF_NORMALIZE_BASIC_COUNTS, default="NONE"): cv.enum(
                NORMALIZATION_OPTIONS, upper=True
            ),
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
            cv.Optional(CONF_IRRADIANCE_PHOTOPIC): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_IRRADIANCE,
                    icon=ICON_IRRADIANCE_PHOTOPIC,
                    accuracy_decimals=3,
                    device_class=DEVICE_CLASS_IRRADIANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_IRRADIANCE_PAR): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_IRRADIANCE,
                    icon=ICON_PAR,
                    accuracy_decimals=3,
                    device_class=DEVICE_CLASS_IRRADIANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_PPFD): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_PPFD,
                    icon=ICON_PPFD,
                    accuracy_decimals=3,
                    device_class=DEVICE_CLASS_EMPTY,
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
            cv.Optional(CONF_RGB_HEX): cv.maybe_simple_value(
                text_sensor.text_sensor_schema(icon=ICON_PALETTE),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_SATURATION_LEVEL): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_PERCENT,
                    icon=ICON_SATURATION,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_EMPTY,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x39))
)

_AS7343_SCHEMA = _COMMON_SCHEMA.extend(
    {
        cv.Optional(CONF_GAIN, default="X8"): cv.enum(GAIN_OPTIONS_43),
        cv.Optional(CONF_COUNTS): COUNTS_SCHEMA_43,
        cv.Optional(CONF_BASIC_COUNTS): BASIC_COUNTS_SCHEMA_43,
        cv.Optional(CONF_CALIBRATION, default={}): CALIBRATION_SCHEMA_43,
    }
)


def _validate_exposure(config: ConfigType) -> ConfigType:
    if config[CONF_ATIME] == 0 and config[CONF_ASTEP] == 0:
        raise cv.Invalid(f"{CONF_ATIME} and {CONF_ASTEP} must not both be 0")
    return config


CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            MODEL_AS7341: _COMMON_SCHEMA.extend(
                {
                    cv.Optional(CONF_GAIN, default="X8"): cv.enum(GAIN_OPTIONS_41),
                    cv.Optional(CONF_COUNTS): COUNTS_SCHEMA_41,
                    cv.Optional(CONF_BASIC_COUNTS): BASIC_COUNTS_SCHEMA_41,
                    cv.Optional(CONF_CALIBRATION, default={}): CALIBRATION_SCHEMA_41,
                }
            ),
            MODEL_AS7343: _AS7343_SCHEMA,
            MODEL_TCS3448: _AS7343_SCHEMA.extend(
                i2c.i2c_device_schema(TCS3448_ADDRESS)
            ),
        },
        upper=True,
        enum=AS734X_MODELS,
    ),
    _validate_exposure,
)


DERIVED_SENSORS = (
    CONF_SATURATION_LEVEL,
    CONF_ILLUMINANCE,
    CONF_IRRADIANCE_PHOTOPIC,
    CONF_IRRADIANCE_PAR,
    CONF_PPFD,
    CONF_COLOR_TEMPERATURE,
)


async def to_code(config: ConfigType) -> None:
    model = config[CONF_TYPE]
    driver = MODEL_AS7343 if model == MODEL_TCS3448 else model
    cg.add_build_flag("-DUSE_" + str(driver))

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.setup_model(config[CONF_TYPE]))
    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_atime(config[CONF_ATIME]))
    cg.add(var.set_astep(config[CONF_ASTEP]))
    cg.add(var.set_normalize_basic_counts(config[CONF_NORMALIZE_BASIC_COUNTS]))

    calibration = config[CONF_CALIBRATION]
    cg.add(var.set_dark_current(calibration[CONF_DARK_CURRENT]))
    cg.add(var.set_channel_correction(calibration[CONF_CHANNEL_CORRECTION]))
    cg.add(var.set_glass_attenuation_factor(calibration[CONF_GLASS_ATTENUATION_FACTOR]))

    bands = BANDS_41 if model == MODEL_AS7341 else BANDS_43

    counts_config = config.get(CONF_COUNTS)
    basic_counts_config = config.get(CONF_BASIC_COUNTS)
    for i, band in enumerate(bands):
        if counts_config and (sensor_config := counts_config.get(band)):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(var.set_counts_sensor(sens, i))
        if basic_counts_config and (sensor_config := basic_counts_config.get(band)):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(var.set_basic_counts_sensor(sens, i))

    for key in DERIVED_SENSORS:
        if sensor_config := config.get(key):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(getattr(var, f"set_{key}_sensor")(sens))

    if rgb_config := config.get(CONF_RGB_HEX):
        cg.add_build_flag("-DUSE_AS734X_RGB")
        sens = await text_sensor.new_text_sensor(rgb_config)
        cg.add(var.set_rgb_hex_sensor(sens))
