import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_GAIN,
    CONF_ID,
    CONF_NAME,
    CONF_ILLUMINANCE,
    DEVICE_CLASS_ILLUMINANCE,
    ICON_BRIGHTNESS_5,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
)


CODEOWNERS = ["@mrgnr", "@latonita"]
DEPENDENCIES = ["i2c"]

as7343_ns = cg.esphome_ns.namespace("as7343")

AS7343Component = as7343_ns.class_(
    "AS7343Component", cg.PollingComponent, i2c.I2CDevice
)

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
CONF_CLEAR = "clear"
CONF_IRRADIANCE = "irradiance"
CONF_PPFD = "ppfd"
CONF_SATURATION = "saturation"

UNIT_COUNTS = "#"
UNIT_IRRADIANCE = "W/m²"
UNIT_PPFD = "µmol/s⋅m²"

ICON_SATURATION = "mdi:weather-sunny-alert"

AS7343_GAIN = as7343_ns.enum("AS7343Gain")
GAIN_OPTIONS = {
    "X0.5": AS7343_GAIN.AS7343_GAIN_0_5X,
    "X1": AS7343_GAIN.AS7343_GAIN_1X,
    "X2": AS7343_GAIN.AS7343_GAIN_2X,
    "X4": AS7343_GAIN.AS7343_GAIN_4X,
    "X8": AS7343_GAIN.AS7343_GAIN_8X,
    "X16": AS7343_GAIN.AS7343_GAIN_16X,
    "X32": AS7343_GAIN.AS7343_GAIN_32X,
    "X64": AS7343_GAIN.AS7343_GAIN_64X,
    "X128": AS7343_GAIN.AS7343_GAIN_128X,
    "X256": AS7343_GAIN.AS7343_GAIN_256X,
    "X512": AS7343_GAIN.AS7343_GAIN_512X,
    "X1024": AS7343_GAIN.AS7343_GAIN_1024X,
    "X2048": AS7343_GAIN.AS7343_GAIN_2048X,
}


SENSOR_SCHEMA = cv.maybe_simple_value(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_COUNTS,
        icon=ICON_BRIGHTNESS_5,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_ILLUMINANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    key=CONF_NAME,
)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AS7343Component),
            cv.Optional(CONF_F1): SENSOR_SCHEMA,
            cv.Optional(CONF_F2): SENSOR_SCHEMA,
            cv.Optional(CONF_FZ): SENSOR_SCHEMA,
            cv.Optional(CONF_F3): SENSOR_SCHEMA,
            cv.Optional(CONF_F4): SENSOR_SCHEMA,
            cv.Optional(CONF_FY): SENSOR_SCHEMA,
            cv.Optional(CONF_F5): SENSOR_SCHEMA,
            cv.Optional(CONF_FXL): SENSOR_SCHEMA,
            cv.Optional(CONF_F6): SENSOR_SCHEMA,
            cv.Optional(CONF_F7): SENSOR_SCHEMA,
            cv.Optional(CONF_F8): SENSOR_SCHEMA,
            cv.Optional(CONF_NIR): SENSOR_SCHEMA,
            cv.Optional(CONF_CLEAR): SENSOR_SCHEMA,
            cv.Optional(CONF_GAIN, default="X8"): cv.enum(GAIN_OPTIONS),
            cv.Optional(CONF_ATIME, default=29): cv.int_range(min=0, max=255),
            cv.Optional(CONF_ASTEP, default=599): cv.int_range(min=0, max=65534),
            cv.Optional(CONF_ILLUMINANCE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_LUX,
                    icon=ICON_BRIGHTNESS_5,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_IRRADIANCE): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_IRRADIANCE,
                    icon=ICON_BRIGHTNESS_5,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_PPFD): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_PPFD,
                    icon=ICON_BRIGHTNESS_5,
                    accuracy_decimals=0,
                    device_class=DEVICE_CLASS_ILLUMINANCE,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                key=CONF_NAME,
            ),
            cv.Optional(CONF_SATURATION): cv.maybe_simple_value(
                sensor.sensor_schema(
                    unit_of_measurement=UNIT_COUNTS,
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

SENSORS = {
    CONF_F1: "set_f1_sensor",
    CONF_F2: "set_f2_sensor",
    CONF_FZ: "set_fz_sensor",
    CONF_F3: "set_f3_sensor",
    CONF_F4: "set_f4_sensor",
    CONF_FY: "set_fy_sensor",
    CONF_F5: "set_f5_sensor",
    CONF_FXL: "set_fxl_sensor",
    CONF_F6: "set_f6_sensor",
    CONF_F7: "set_f7_sensor",
    CONF_F8: "set_f8_sensor",
    CONF_NIR: "set_nir_sensor",
    CONF_CLEAR: "set_clear_sensor",
    CONF_ILLUMINANCE: "set_illuminance_sensor",
    CONF_IRRADIANCE: "set_irradiance_sensor",
    CONF_PPFD: "set_ppfd_sensor",
    CONF_SATURATION: "set_saturation_sensor",
}


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_atime(config[CONF_ATIME]))
    cg.add(var.set_astep(config[CONF_ASTEP]))

    for conf_id, set_sensor_func in SENSORS.items():
        if sens_config := config.get(conf_id):
            sens = await sensor.new_sensor(sens_config)
            cg.add(getattr(var, set_sensor_func)(sens))
