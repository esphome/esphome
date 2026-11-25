"""TMP102 temperature sensor and optional hardware configuration."""

import esphome.codegen as cg
from esphome.components import i2c, sensor
from esphome.components.const import CONF_CONVERSION_RATE
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from . import (
    ALERT_POLARITIES,
    CONF_ALERT_POLARITY,
    CONF_EXTENDED_MODE,
    CONF_FAULT_QUEUE,
    CONF_ONE_SHOT_MODE,
    CONF_TEMPERATURE_HIGH,
    CONF_TEMPERATURE_LOW,
    CONF_THERMOSTAT_MODE,
    CONVERSION_RATES,
    FAULT_QUEUE_BITS,
    THERMOSTAT_MODES,
    TMP102_DEFAULT_THIGH,
    TMP102_DEFAULT_TLOW,
    TMP102Component,
    build_configuration,
    encode_temperature,
    number_parent_extended,
    validate_tmp102_thresholds,
)

CODEOWNERS = ["@timsavage"]
DEPENDENCIES = ["i2c"]

# No defaults here: absence of all advanced options preserves upstream's read-only behavior and firmware size.
_ADVANCED_SCHEMA = {
    cv.Optional(CONF_EXTENDED_MODE): cv.boolean,
    cv.Optional(CONF_CONVERSION_RATE): cv.enum(CONVERSION_RATES),
    cv.Optional(CONF_ONE_SHOT_MODE): cv.boolean,
    cv.Optional(CONF_ALERT_POLARITY): cv.enum(ALERT_POLARITIES),
    cv.Optional(CONF_THERMOSTAT_MODE): cv.enum(THERMOSTAT_MODES),
    cv.Optional(CONF_FAULT_QUEUE): cv.enum(FAULT_QUEUE_BITS, int=True),
    cv.Optional(CONF_TEMPERATURE_HIGH): cv.temperature,
    cv.Optional(CONF_TEMPERATURE_LOW): cv.temperature,
}

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(TMP102Component, kind=sensor.TEMPERATURE)
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x48))
    .extend(_ADVANCED_SCHEMA),
    validate_tmp102_thresholds,
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    has_numbers = number_parent_extended(config[CONF_ID]) is not None
    if not has_numbers and not any(key in config for key in _ADVANCED_SCHEMA):
        return

    extended = config.get(CONF_EXTENDED_MODE, False)
    cg.add_define("USE_TMP102_CONFIGURE")
    cg.add(
        var.set_configuration(
            build_configuration(config),
            encode_temperature(
                config.get(CONF_TEMPERATURE_HIGH, TMP102_DEFAULT_THIGH), extended
            ),
            encode_temperature(
                config.get(CONF_TEMPERATURE_LOW, TMP102_DEFAULT_TLOW), extended
            ),
        )
    )
