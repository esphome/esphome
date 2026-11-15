from collections.abc import Callable
from typing import Any

import esphome.codegen as cg
from esphome.components import i2c, sensor
from esphome.components.const import UNIT_COUNTS
import esphome.config_validation as cv
from esphome.const import (
    CONF_AMBIENT_LIGHT,
    CONF_GAIN,
    CONF_LIGHT,
    CONF_RESOLUTION,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_ILLUMINANCE,
    ICON_BRIGHTNESS_5,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
)
from esphome.types import ConfigType

CODEOWNERS = ["@sjtrny", "@latonita"]
DEPENDENCIES = ["i2c"]

ltr390_ns = cg.esphome_ns.namespace("ltr390")

LTR390Component = ltr390_ns.class_(
    "LTR390Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_UV_INDEX = "uv_index"
CONF_UV = "uv"
CONF_WINDOW_CORRECTION_FACTOR = "window_correction_factor"

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


def _per_mode(validator: Callable[[Any], Any]) -> Callable[[Any], ConfigType]:
    """One value for both modes, or one each; always returns the per-mode dict."""
    return cv.Any(
        cv.All(validator, lambda value: {CONF_AMBIENT_LIGHT: value, CONF_UV: value}),
        cv.Schema(
            {
                cv.Required(CONF_AMBIENT_LIGHT): validator,
                cv.Required(CONF_UV): validator,
            }
        ),
    )


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LTR390Component),
            cv.Optional(CONF_LIGHT): sensor.sensor_schema(
                unit_of_measurement=UNIT_LUX,
                icon=ICON_BRIGHTNESS_5,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ILLUMINANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AMBIENT_LIGHT): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS,
                icon=ICON_BRIGHTNESS_5,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_EMPTY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_UV_INDEX): sensor.sensor_schema(
                unit_of_measurement=UNIT_UVI,
                icon=ICON_BRIGHTNESS_5,
                accuracy_decimals=5,
                device_class=DEVICE_CLASS_EMPTY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_UV): sensor.sensor_schema(
                unit_of_measurement=UNIT_COUNTS,
                icon=ICON_BRIGHTNESS_5,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_EMPTY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_GAIN, default="X18"): _per_mode(cv.enum(GAIN_OPTIONS)),
            cv.Optional(CONF_RESOLUTION, default=20): _per_mode(cv.enum(RES_OPTIONS)),
            cv.Optional(CONF_WINDOW_CORRECTION_FACTOR, default=1.0): cv.float_range(
                min=1.0
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x53)),
    cv.has_at_least_one_key(CONF_LIGHT, CONF_AMBIENT_LIGHT, CONF_UV_INDEX, CONF_UV),
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    cg.add(var.set_wfac_value(config[CONF_WINDOW_CORRECTION_FACTOR]))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_LIGHT, var.set_light_sensor)
    await sensors(CONF_AMBIENT_LIGHT, var.set_als_sensor)
    await sensors(CONF_UV_INDEX, var.set_uvi_sensor)
    await sensors(CONF_UV, var.set_uv_sensor)

    gain = config[CONF_GAIN]
    cg.add(var.set_als_gain_value(gain[CONF_AMBIENT_LIGHT]))
    cg.add(var.set_uv_gain_value(gain[CONF_UV]))
    res = config[CONF_RESOLUTION]
    cg.add(var.set_als_res_value(res[CONF_AMBIENT_LIGHT]))
    cg.add(var.set_uv_res_value(res[CONF_UV]))
