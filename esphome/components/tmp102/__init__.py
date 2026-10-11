"""Shared TMP102 types and configuration validation."""

import esphome.codegen as cg
from esphome.components import i2c, sensor
from esphome.components.const import CONF_CONVERSION_RATE
import esphome.config_validation as cv
from esphome.types import ConfigType

DOMAIN = "tmp102"

tmp102_ns = cg.esphome_ns.namespace("tmp102")
TMP102Component = tmp102_ns.class_(
    "TMP102Component", cg.PollingComponent, i2c.I2CDevice, sensor.Sensor
)

CONF_EXTENDED_MODE = "extended_mode"
CONF_ONE_SHOT_MODE = "one_shot_mode"
CONF_TEMPERATURE_HIGH = "temperature_high"
CONF_TEMPERATURE_LOW = "temperature_low"
CONF_ALERT_POLARITY = "alert_polarity"
CONF_THERMOSTAT_MODE = "thermostat_mode"
CONF_FAULT_QUEUE = "fault_queue"

CONVERSION_RATES = {"0.25Hz": 0, "1Hz": 1, "4Hz": 2, "8Hz": 3}
THERMOSTAT_MODES = {"comparator": 0, "interrupt": 1}
ALERT_POLARITIES = {"active_low": 0, "active_high": 1}
FAULT_QUEUE_BITS = {1: 0, 2: 1, 4: 2, 6: 3}

TMP102_CONVERSION_FACTOR = 0.0625
TMP102_DEFAULT_THIGH = 80.0
TMP102_DEFAULT_TLOW = 75.0


def encode_temperature(temperature: float, extended: bool) -> int:
    """Encode a temperature as a TMP102 limit-register word."""
    steps = round(temperature / TMP102_CONVERSION_FACTOR)
    return (steps << (3 if extended else 4)) & 0xFFFF


def build_configuration(config: ConfigType) -> int:
    """Build the writable TMP102 configuration-register bits."""
    conversion_rate = config.get(CONF_CONVERSION_RATE)
    alert_polarity = config.get(CONF_ALERT_POLARITY)
    thermostat_mode = config.get(CONF_THERMOSTAT_MODE)
    value = (
        conversion_rate.enum_value
        if conversion_rate is not None
        else CONVERSION_RATES["4Hz"]
    ) << 6
    value |= FAULT_QUEUE_BITS[config.get(CONF_FAULT_QUEUE, 1)] << 11
    value |= (
        alert_polarity.enum_value
        if alert_polarity is not None
        else ALERT_POLARITIES["active_low"]
    ) << 10
    value |= (
        thermostat_mode.enum_value
        if thermostat_mode is not None
        else THERMOSTAT_MODES["comparator"]
    ) << 9
    if config.get(CONF_ONE_SHOT_MODE, False):
        value |= 1 << 8
    if config.get(CONF_EXTENDED_MODE, False):
        value |= 1 << 4
    return value


def _validate_temperature_range(key: str, value: float, extended: bool) -> None:
    t_min = -55.0
    t_max = 150.0 if extended else 127.9375
    mode_str = "extended" if extended else "normal"
    if not (t_min <= value <= t_max):
        raise cv.Invalid(
            f"{key} ({value}°C) out of range for {mode_str} mode "
            f"({t_min}°C to {t_max}°C)",
            [key],
        )


def validate_tmp102_thresholds(config: ConfigType) -> ConfigType:
    extended = config.get(CONF_EXTENDED_MODE, False)
    for key in (CONF_TEMPERATURE_HIGH, CONF_TEMPERATURE_LOW):
        if key in config:
            _validate_temperature_range(key, config[key], extended)

    high = config.get(CONF_TEMPERATURE_HIGH)
    low = config.get(CONF_TEMPERATURE_LOW)
    if high is not None and low is not None and low > high:
        raise cv.Invalid(
            f"low limit ({low}°C) must be <= high limit ({high}°C)",
            [CONF_TEMPERATURE_LOW],
        )
    if high is not None and low is None and high < TMP102_DEFAULT_TLOW:
        raise cv.Invalid(
            f"high limit ({high}°C) is below the chip power-up TLOW default "
            f"({TMP102_DEFAULT_TLOW}°C). Set temperature_low explicitly.",
            [CONF_TEMPERATURE_HIGH],
        )
    if low is not None and high is None and low > TMP102_DEFAULT_THIGH:
        raise cv.Invalid(
            f"low limit ({low}°C) is above the chip power-up THIGH default "
            f"({TMP102_DEFAULT_THIGH}°C). Set temperature_high explicitly.",
            [CONF_TEMPERATURE_LOW],
        )
    return config
