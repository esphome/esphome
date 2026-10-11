"""``mics_5524_gas_sensor`` - ESPHome sensor platform for the MiCS-5524.

One entry configures one gas of one sensor.  The MiCS-5524 has a single analog
output, so several entries on the same physical sensor report the same mixture
with different calibrations (see ``SELECTIVITY_NOTE``).

The signal comes from an existing voltage sampler (``voltage:``, e.g. an ``adc``
sensor configured next to this one).
"""

import logging

import esphome.codegen as cg
from esphome.components import sensor, voltage_sampler
import esphome.config_validation as cv
from esphome.const import (
    CONF_CALIBRATION,
    CONF_DELAY,
    CONF_VOLTAGE,
    CONF_WARMUP_TIME,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)
from esphome.types import ConfigType

from . import (
    CONF_AIR_REFERENCE,
    CONF_GAS,
    CONF_MAX_PPM,
    CONF_MIN_PPM,
    CONF_SAMPLE_COUNT,
    CONF_SAMPLE_INTERVAL,
    CONF_VCC,
    CONF_VOLTAGE_MULTIPLIER,
    MiCS5524GasSensor,
)
from .coefficients import (
    SELECTIVITY_NOTE,
    gas_keys,
    label_for,
    resolve_gas,
    resolve_gas_key,
)

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["sensor", "voltage_sampler"]


def validate_gas(value: str) -> str:
    """Accept ``h2`` / ``H-2`` / ``hydrogen`` and return the canonical gas key."""
    key = resolve_gas_key(value)
    if key not in gas_keys():
        raise cv.Invalid(
            f"'{value}' is not a supported MiCS-5524 gas, expected one of: "
            f"{', '.join(gas_keys())}"
        )
    return key


CALIBRATION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DELAY, default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SAMPLE_COUNT, default=10): cv.int_range(min=1, max=1000),
    }
)


def _validate_config(config: ConfigType) -> ConfigType:
    """Cross-check the gas and the PPM range at compile time."""
    gas = config[CONF_GAS]
    label = label_for(gas)

    try:
        resolved = resolve_gas(
            gas,
            min_ppm=config.get(CONF_MIN_PPM),
            max_ppm=config.get(CONF_MAX_PPM),
        )
    except ValueError as err:
        raise cv.Invalid(str(err)) from err

    if resolved.max_ppm <= resolved.min_ppm:
        raise cv.Invalid(
            f"max_ppm ({resolved.max_ppm}) must be greater than min_ppm ({resolved.min_ppm})"
        )

    fixed = CONF_AIR_REFERENCE
    if fixed in config and CONF_CALIBRATION in config:
        _LOGGER.warning(
            "%s: both '%s:' and 'calibration:' are set - the fixed value wins and the "
            "automatic calibration at boot is disabled",
            label,
            fixed,
        )
    if fixed not in config and CONF_CALIBRATION not in config:
        _LOGGER.warning(
            "%s: neither '%s:' nor 'calibration:' is set - the PPM value stays unknown "
            "until the sensor has been calibrated in clean air",
            label,
            fixed,
        )

    if resolved.note:
        _LOGGER.info("%s: %s", label, resolved.note)
    _LOGGER.info(
        "%s: %s (treat the value as an estimate)",
        label,
        SELECTIVITY_NOTE,
    )

    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        MiCS5524GasSensor,
        unit_of_measurement=UNIT_PARTS_PER_MILLION,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:molecule",
    )
    .extend(
        {
            cv.Required(CONF_GAS): cv.All(cv.string, validate_gas),
            cv.Required(CONF_VOLTAGE): cv.use_id(voltage_sampler.VoltageSampler),
            cv.Optional(CONF_VOLTAGE_MULTIPLIER): cv.positive_not_null_float,
            cv.Optional(CONF_VCC, default=5.0): cv.positive_not_null_float,
            cv.Optional(CONF_AIR_REFERENCE): cv.positive_not_null_float,
            cv.Optional(CONF_SAMPLE_COUNT, default=4): cv.int_range(min=1, max=255),
            cv.Optional(
                CONF_SAMPLE_INTERVAL, default="20ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_WARMUP_TIME, default="3min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MIN_PPM): cv.float_,
            cv.Optional(CONF_MAX_PPM): cv.positive_not_null_float,
            cv.Optional(CONF_CALIBRATION): CALIBRATION_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("30s")),
    _validate_config,
)


async def to_code(config: ConfigType) -> None:
    resolved = resolve_gas(
        config[CONF_GAS],
        min_ppm=config.get(CONF_MIN_PPM),
        max_ppm=config.get(CONF_MAX_PPM),
    )

    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    cg.add(var.set_gas(resolved.label))
    cg.add(var.set_threshold(resolved.threshold))
    cg.add(var.set_gain(resolved.gain))
    cg.add(var.set_vendor_min_ppm(resolved.vendor_min_ppm))
    cg.add(var.set_vendor_max_ppm(resolved.vendor_max_ppm))
    cg.add(var.set_min_ppm(resolved.min_ppm))
    cg.add(var.set_max_ppm(resolved.max_ppm))
    cg.add(var.set_vcc(config[CONF_VCC]))
    cg.add(var.set_voltage_multiplier(float(config.get(CONF_VOLTAGE_MULTIPLIER, 1.0))))
    cg.add(var.set_samples(config[CONF_SAMPLE_COUNT]))
    cg.add(var.set_sample_interval(config[CONF_SAMPLE_INTERVAL]))
    cg.add(var.set_warmup_time(config[CONF_WARMUP_TIME]))

    if (air_reference := config.get(CONF_AIR_REFERENCE)) is not None:
        cg.add(var.set_air_reference(air_reference))

    fixed = CONF_AIR_REFERENCE
    calibration = config.get(CONF_CALIBRATION)
    if calibration is not None and fixed not in config:
        cg.add(
            var.set_calibration(
                True,
                calibration[CONF_DELAY],
                calibration[CONF_SAMPLE_COUNT],
            )
        )

    cg.add(var.set_source(await cg.get_variable(config[CONF_VOLTAGE])))

    _LOGGER.debug(
        "%s: threshold=%s gain=%s vcc=%s range=%s..%s ppm",
        resolved.label,
        resolved.threshold,
        resolved.gain,
        config[CONF_VCC],
        resolved.min_ppm,
        resolved.max_ppm,
    )
