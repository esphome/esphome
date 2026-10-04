import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLEAR,
    CONF_COUNTS,
    CONF_GAIN,
    CONF_ID,
    CONF_NAME,
    CONF_TYPE,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_MEASUREMENT,
)
from esphome.types import ConfigType

CODEOWNERS = ["@latonita", "@mrgnr"]
DEPENDENCIES = ["i2c"]

CONF_ATIME = "atime"
CONF_ASTEP = "astep"

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
ICON_COUNTS = "mdi:counter"

MODEL_AS7341 = "AS7341"
MODEL_AS7343 = "AS7343"
MODEL_TCS3448 = "TCS3448"

as734x_ns = cg.esphome_ns.namespace("as734x")
AS734XComponent = as734x_ns.class_(
    "AS734XComponent", cg.PollingComponent, i2c.I2CDevice
)

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

_COMMON_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AS734XComponent),
            cv.Optional(CONF_ATIME, default=29): cv.int_range(min=0, max=255),
            cv.Optional(CONF_ASTEP, default=599): cv.int_range(min=0, max=65534),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x39))
)

_AS7343_SCHEMA = _COMMON_SCHEMA.extend(
    {
        cv.Optional(CONF_GAIN, default="X8"): cv.enum(GAIN_OPTIONS_43),
        cv.Optional(CONF_COUNTS): COUNTS_SCHEMA_43,
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


async def to_code(config):
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

    bands = BANDS_41 if model == MODEL_AS7341 else BANDS_43

    if counts_config := config.get(CONF_COUNTS):
        for i, band in enumerate(bands):
            if sensor_config := counts_config.get(band):
                sens = await sensor.new_sensor(sensor_config)
                cg.add(var.set_counts_sensor(sens, i))
