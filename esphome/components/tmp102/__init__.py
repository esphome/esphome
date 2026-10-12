"""Shared TMP102 types and configuration validation."""

import esphome.codegen as cg
from esphome.components import i2c, sensor
from esphome.components.const import CONF_CONVERSION_RATE
import esphome.config_validation as cv
from esphome.const import CONF_PLATFORM
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

CONF_TMP102_ID = "tmp102_id"

DOMAIN = "tmp102"

tmp102_ns = cg.esphome_ns.namespace("tmp102")
TMP102Component = tmp102_ns.class_(
    "TMP102Component", cg.PollingComponent, i2c.I2CDevice, sensor.Sensor
)
TMP102LimitType = tmp102_ns.enum("TMP102LimitType")

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

_KEY_NUMBER_CONFIGURATION = "number_configuration"


def set_number_configuration(
    parent_id: object, extended: bool, high: float, low: float
) -> None:
    """Store resolved number limits for sensor and number code generation."""
    configurations = CORE.data.setdefault(DOMAIN, {}).setdefault(
        _KEY_NUMBER_CONFIGURATION, {}
    )
    configurations[str(parent_id)] = {
        CONF_EXTENDED_MODE: extended,
        CONF_TEMPERATURE_HIGH: high,
        CONF_TEMPERATURE_LOW: low,
    }


def get_number_configuration(parent_id: object) -> dict[str, bool | float] | None:
    """Return resolved number limits for a TMP102 sensor."""
    return (
        CORE.data.get(DOMAIN, {})
        .get(_KEY_NUMBER_CONFIGURATION, {})
        .get(str(parent_id))
    )


def encode_temperature(temperature: float, extended: bool) -> int:
    """Encode a temperature as a TMP102 limit-register word."""
    steps = round(temperature / TMP102_CONVERSION_FACTOR)
    return (steps << (3 if extended else 4)) & 0xFFFF


def build_configuration(config: ConfigType) -> int:
    """Build the writable TMP102 configuration-register bits."""
    value = CONVERSION_RATES[config.get(CONF_CONVERSION_RATE, "4Hz")] << 6
    value |= FAULT_QUEUE_BITS[config.get(CONF_FAULT_QUEUE, 1)] << 11
    value |= ALERT_POLARITIES[config.get(CONF_ALERT_POLARITY, "active_low")] << 10
    value |= THERMOSTAT_MODES[config.get(CONF_THERMOSTAT_MODE, "comparator")] << 9
    if config.get(CONF_ONE_SHOT_MODE, False):
        value |= 1 << 8
    if config.get(CONF_EXTENDED_MODE, False):
        value |= 1 << 4
    return value


def validate_tmp102_thresholds(config: ConfigType) -> ConfigType:
    extended = config.get(CONF_EXTENDED_MODE, False)
    t_min = -55.0
    t_max = 150.0 if extended else 127.9375
    high = config.get(CONF_TEMPERATURE_HIGH)
    low = config.get(CONF_TEMPERATURE_LOW)
    for key, value in ((CONF_TEMPERATURE_HIGH, high), (CONF_TEMPERATURE_LOW, low)):
        if value is not None and not t_min <= value <= t_max:
            raise cv.Invalid(
                f"{key} ({value}°C) out of range for "
                f"{'extended' if extended else 'normal'} mode "
                f"({t_min}°C to {t_max}°C)",
                [key],
            )

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


def validate_unique_child(config: ConfigType, platform: str) -> ConfigType:
    entries = fv.full_config.get().get(platform, [])
    matches = [
        entry
        for entry in entries
        if entry.get(CONF_PLATFORM) == DOMAIN
        and entry.get(CONF_TMP102_ID) == config[CONF_TMP102_ID]
    ]
    if len(matches) > 1:
        raise cv.Invalid(
            f"Only one TMP102 {platform} may reference each temperature sensor"
        )
    return config
