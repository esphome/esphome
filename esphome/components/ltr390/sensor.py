import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_GAIN,
    CONF_LIGHT,
    CONF_RESOLUTION,
    UNIT_LUX,
    ICON_BRIGHTNESS_5,
    DEVICE_CLASS_ILLUMINANCE,
)

CODEOWNERS = ["@sjtrny"]
DEPENDENCIES = ["i2c"]

ltr390_ns = cg.esphome_ns.namespace("ltr390")

LTR390Component = ltr390_ns.class_(
    "LTR390Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_AMBIENT_LIGHT = "ambient_light"
CONF_UV_INDEX = "uv_index"
CONF_UV = "uv"
CONF_WINDOW_CORRECTION_FACTOR = "window_correction_factor"

UNIT_COUNTS = "#"
UNIT_UVI = "UVI"

LTR390GAIN = ltr390_ns.enum("LTR390GAIN")
GAIN_OPTIONS = {
    "X1": LTR390GAIN.LTR390_GAIN_1,
    "X3": LTR390GAIN.LTR390_GAIN_3,
    "X6": LTR390GAIN.LTR390_GAIN_6,
    "X9": LTR390GAIN.LTR390_GAIN_9,
    "X18": LTR390GAIN.LTR390_GAIN_18,
}

LTR390RESOLUTION = ltr390_ns.enum("LTR390RESOLUTION")
RES_OPTIONS = {
    20: LTR390RESOLUTION.LTR390_RESOLUTION_20BIT,
    19: LTR390RESOLUTION.LTR390_RESOLUTION_19BIT,
    18: LTR390RESOLUTION.LTR390_RESOLUTION_18BIT,
    17: LTR390RESOLUTION.LTR390_RESOLUTION_17BIT,
    16: LTR390RESOLUTION.LTR390_RESOLUTION_16BIT,
    13: LTR390RESOLUTION.LTR390_RESOLUTION_13BIT,
}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LTR390Component),
            cv.Optional(CONF_LIGHT): sensor.sensor_schema(
                unit_of_measurement=UNIT_LUX,
                icon=ICON_BRIGHTNESS_5,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ILLUMINANCE,
            ),
            cv.Optional(CONF_AMBIENT_LIGHT): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS,
                icon=ICON_BRIGHTNESS_5,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ILLUMINANCE,
            ),
            cv.Optional(CONF_UV_INDEX): sensor.sensor_schema(
                unit_of_measurement=UNIT_UVI,
                icon=ICON_BRIGHTNESS_5,
                accuracy_decimals=5,
                device_class=DEVICE_CLASS_ILLUMINANCE,
            ),
            cv.Optional(CONF_UV): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS,
                icon=ICON_BRIGHTNESS_5,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ILLUMINANCE,
            ),
            cv.Optional(CONF_GAIN, default="X3"): cv.enum(GAIN_OPTIONS),
            cv.Optional(CONF_RESOLUTION, default=18): cv.enum(RES_OPTIONS),
            cv.Optional(CONF_WINDOW_CORRECTION_FACTOR, default=1.0): cv.float_range(
                min=1.0
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x53)),
    cv.has_at_least_one_key(CONF_LIGHT, CONF_AMBIENT_LIGHT, CONF_UV_INDEX, CONF_UV),
)

TYPES = {
    CONF_LIGHT: "set_light_sensor",
    CONF_AMBIENT_LIGHT: "set_als_sensor",
    CONF_UV_INDEX: "set_uvi_sensor",
    CONF_UV: "set_uv_sensor",
}


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_gain_value(config[CONF_GAIN]))
    cg.add(var.set_res_value(config[CONF_RESOLUTION]))
    cg.add(var.set_wfac_value(config[CONF_WINDOW_CORRECTION_FACTOR]))

    for key, funcName in TYPES.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, funcName)(sens))
