"""Shared TMP102 types and configuration validation."""

import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INITIAL_VALUE
import esphome.final_validate as fv
from esphome.types import ConfigType

CONF_TMP102_ID = "tmp102_id"
PLATFORMS = ("tmp102",)

tmp102_ns = cg.esphome_ns.namespace("tmp102")
TMP102Component = tmp102_ns.class_(
    "TMP102Component", cg.PollingComponent, i2c.I2CDevice, sensor.Sensor
)

TMP102ConversionRate = tmp102_ns.enum("TMP102ConversionRate")
TMP102ThermostatMode = tmp102_ns.enum("TMP102ThermostatMode")
TMP102AlertPolarity = tmp102_ns.enum("TMP102AlertPolarity")
TMP102LimitType = tmp102_ns.enum("TMP102LimitType")

CONF_EXTENDED_MODE = "extended_mode"
CONF_CONVERSION_RATE = "conversion_rate"
CONF_ONE_SHOT_MODE = "one_shot_mode"
CONF_ALERT = "alert"
CONF_THRESHOLD_STATUS = "threshold_status"
CONF_TEMPERATURE_HIGH = "temperature_high"
CONF_TEMPERATURE_LOW = "temperature_low"
CONF_ALERT_POLARITY = "alert_polarity"
CONF_THERMOSTAT_MODE = "thermostat_mode"
CONF_FAULT_QUEUE = "fault_queue"

CONVERSION_RATES = {
    "0.25Hz": TMP102ConversionRate.TMP102_CONVERSION_RATE_0_25HZ,
    "1Hz": TMP102ConversionRate.TMP102_CONVERSION_RATE_1HZ,
    "4Hz": TMP102ConversionRate.TMP102_CONVERSION_RATE_4HZ,
    "8Hz": TMP102ConversionRate.TMP102_CONVERSION_RATE_8HZ,
}

THERMOSTAT_MODES = {
    "comparator": TMP102ThermostatMode.TMP102_THERMOSTAT_MODE_COMPARATOR,
    "interrupt": TMP102ThermostatMode.TMP102_THERMOSTAT_MODE_INTERRUPT,
}

ALERT_POLARITIES = {
    "active_low": TMP102AlertPolarity.TMP102_ALERT_POLARITY_ACTIVE_LOW,
    "active_high": TMP102AlertPolarity.TMP102_ALERT_POLARITY_ACTIVE_HIGH,
}

LIMIT_TYPES = {
    CONF_TEMPERATURE_LOW: TMP102LimitType.TMP102_LIMIT_LOW,
    CONF_TEMPERATURE_HIGH: TMP102LimitType.TMP102_LIMIT_HIGH,
}

# Chip power-up defaults used when the user omits one or both limit registers.
_TMP102_DEFAULT_THIGH = 80.0
_TMP102_DEFAULT_TLOW = 75.0


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
    # Normal mode: datasheet format table covers -55°C to +127.9375°C (12-bit signed, 0.0625°C/LSB).
    # Extended mode: datasheet spec is -55°C to +150°C.
    for key in (CONF_TEMPERATURE_HIGH, CONF_TEMPERATURE_LOW):
        if key not in config:
            continue
        value = config[key]
        _validate_temperature_range(key, value, extended)

    high_initial = config.get(CONF_TEMPERATURE_HIGH)
    low_initial = config.get(CONF_TEMPERATURE_LOW)

    if (
        high_initial is not None
        and low_initial is not None
        and low_initial > high_initial
    ):
        raise cv.Invalid(
            f"initial low limit ({low_initial}°C) must be <= initial high limit ({high_initial}°C)",
            [CONF_TEMPERATURE_LOW],
        )
    if (
        high_initial is not None
        and low_initial is None
        and high_initial < _TMP102_DEFAULT_TLOW
    ):
        raise cv.Invalid(
            f"initial high limit ({high_initial}°C) is below the chip power-up "
            f"TLOW default ({_TMP102_DEFAULT_TLOW}°C). Set temperature_low explicitly.",
            [CONF_TEMPERATURE_HIGH],
        )
    if (
        low_initial is not None
        and high_initial is None
        and low_initial > _TMP102_DEFAULT_THIGH
    ):
        raise cv.Invalid(
            f"initial low limit ({low_initial}°C) is above the chip power-up "
            f"THIGH default ({_TMP102_DEFAULT_THIGH}°C). Set temperature_high explicitly.",
            [CONF_TEMPERATURE_LOW],
        )

    return config


def get_parent_config(config: ConfigType) -> ConfigType:
    full = fv.full_config.get()
    path = full.get_path_for_id(config[CONF_TMP102_ID])[:-1]
    return full.get_config_for_path(path)


def validate_parent_thresholds(config: ConfigType) -> ConfigType:
    """Resolve initial limits across platforms before checking their relationship."""
    full = fv.full_config.get()
    effective = dict(config)
    seen = set()
    for entry in full.get("number", []):
        if (
            entry.get("platform") not in PLATFORMS
            or entry.get(CONF_TMP102_ID) != config[CONF_ID]
        ):
            continue
        for key in (CONF_TEMPERATURE_HIGH, CONF_TEMPERATURE_LOW):
            if key not in entry:
                continue
            if key in seen:
                raise cv.Invalid(f"Only one {key} number may reference each TMP102")
            seen.add(key)
            if CONF_INITIAL_VALUE in entry[key]:
                if key in config:
                    raise cv.Invalid(
                        f"Set {key} on the sensor or initial_value on its number, not both"
                    )
                effective[key] = entry[key][CONF_INITIAL_VALUE]
    validate_tmp102_thresholds(effective)
    return config


def validate_unique_child(config: ConfigType, platform: str) -> ConfigType:
    entries = fv.full_config.get().get(platform, [])
    matches = [
        entry
        for entry in entries
        if entry.get("platform") in PLATFORMS
        and entry.get(CONF_TMP102_ID) == config[CONF_TMP102_ID]
    ]
    if len(matches) > 1:
        raise cv.Invalid(
            f"Only one TMP102 {platform} may reference each temperature sensor"
        )
    return config
