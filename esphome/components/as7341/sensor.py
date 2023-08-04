import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_GAIN,
    CONF_ID,
    DEVICE_CLASS_ILLUMINANCE,
    ICON_BRIGHTNESS_5,
    STATE_CLASS_MEASUREMENT,
)


CODEOWNERS = ["@mrgnr"]
DEPENDENCIES = ["i2c"]

as7341_ns = cg.esphome_ns.namespace("as7341")

AS7341Component = as7341_ns.class_(
    "AS7341Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_ATIME = "atime"
CONF_ASTEP = "astep"

CONF_F1 = "f1"
CONF_F2 = "f2"
CONF_F3 = "f3"
CONF_F4 = "f4"
CONF_F5 = "f5"
CONF_F6 = "f6"
CONF_F7 = "f7"
CONF_F8 = "f8"
CONF_CLEAR = "clear"
CONF_NIR = "nir"

UNIT_COUNTS = "#"

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


SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_COUNTS,
    icon=ICON_BRIGHTNESS_5,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_ILLUMINANCE,
    state_class=STATE_CLASS_MEASUREMENT,
)


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AS7341Component),
            cv.Optional(CONF_F1): SENSOR_SCHEMA,
            cv.Optional(CONF_F2): SENSOR_SCHEMA,
            cv.Optional(CONF_F3): SENSOR_SCHEMA,
            cv.Optional(CONF_F4): SENSOR_SCHEMA,
            cv.Optional(CONF_F5): SENSOR_SCHEMA,
            cv.Optional(CONF_F6): SENSOR_SCHEMA,
            cv.Optional(CONF_F7): SENSOR_SCHEMA,
            cv.Optional(CONF_F8): SENSOR_SCHEMA,
            cv.Optional(CONF_CLEAR): SENSOR_SCHEMA,
            cv.Optional(CONF_NIR): SENSOR_SCHEMA,
            cv.Optional(CONF_GAIN, default="X8"): cv.enum(GAIN_OPTIONS),
            cv.Optional(CONF_ATIME, default=29): cv.int_range(min=0, max=255),
            cv.Optional(CONF_ASTEP, default=599): cv.int_range(min=0, max=65534),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x39))
)

SENSORS = {
    CONF_F1: "set_f1_sensor",
    CONF_F2: "set_f2_sensor",
    CONF_F3: "set_f3_sensor",
    CONF_F4: "set_f4_sensor",
    CONF_F5: "set_f5_sensor",
    CONF_F6: "set_f6_sensor",
    CONF_F7: "set_f7_sensor",
    CONF_F8: "set_f8_sensor",
    CONF_CLEAR: "set_clear_sensor",
    CONF_NIR: "set_nir_sensor",
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
