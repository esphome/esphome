"""TMP102 temperature sensor and optional hardware configuration."""

import esphome.codegen as cg
from esphome.components import i2c, sensor
from esphome.components.const import CONF_CONVERSION_RATE
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)
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
    THERMOSTAT_MODES,
    TMP102_DEFAULT_THIGH,
    TMP102_DEFAULT_TLOW,
    TMP102Component,
    build_configuration,
    encode_temperature,
    validate_tmp102_thresholds,
)

CODEOWNERS = ["@timsavage"]
DEPENDENCIES = ["i2c"]

ADVANCED_OPTIONS = (
    CONF_EXTENDED_MODE,
    CONF_CONVERSION_RATE,
    CONF_ONE_SHOT_MODE,
    CONF_ALERT_POLARITY,
    CONF_THERMOSTAT_MODE,
    CONF_FAULT_QUEUE,
    CONF_TEMPERATURE_HIGH,
    CONF_TEMPERATURE_LOW,
)

# No defaults here: absence of all advanced options preserves upstream's read-only behavior and firmware size.
CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        TMP102Component,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x48))
    .extend(
        {
            cv.Optional(CONF_EXTENDED_MODE): cv.boolean,
            cv.Optional(CONF_CONVERSION_RATE): cv.enum(CONVERSION_RATES),
            cv.Optional(CONF_ONE_SHOT_MODE): cv.boolean,
            cv.Optional(CONF_ALERT_POLARITY): cv.enum(ALERT_POLARITIES),
            cv.Optional(CONF_THERMOSTAT_MODE): cv.enum(THERMOSTAT_MODES),
            cv.Optional(CONF_FAULT_QUEUE): cv.one_of(1, 2, 4, 6, int=True),
            cv.Optional(CONF_TEMPERATURE_HIGH): cv.temperature,
            cv.Optional(CONF_TEMPERATURE_LOW): cv.temperature,
        }
    ),
    validate_tmp102_thresholds,
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if not any(key in config for key in ADVANCED_OPTIONS):
        return

    extended = config.get(CONF_EXTENDED_MODE, False)
    high = config.get(CONF_TEMPERATURE_HIGH, TMP102_DEFAULT_THIGH)
    low = config.get(CONF_TEMPERATURE_LOW, TMP102_DEFAULT_TLOW)
    configured_limits = (int(CONF_TEMPERATURE_HIGH in config) << 1) | int(
        CONF_TEMPERATURE_LOW in config
    )
    cg.add_define("USE_TMP102_CONFIGURE")
    cg.add(
        var.set_configuration(
            build_configuration(config),
            encode_temperature(high, extended),
            encode_temperature(low, extended),
            configured_limits,
        )
    )
