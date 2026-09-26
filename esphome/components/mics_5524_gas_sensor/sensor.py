"""``mics_5524_gas_sensor`` - ESPHome sensor platform for the MiCS-5524.

One entry configures one gas of one sensor (the MiCS-5524 has a single analog
output, so two entries on the same physical sensor are possible but will report
the same mixture with different calibrations - see ``SELECTIVITY_NOTE``).

The analog signal comes either from an existing voltage sampler (``voltage:``,
e.g. an ``adc`` or ``ads1115`` sensor) or from a pin that this component owns
(``pin:``, which creates a hidden internal ``adc`` sensor).
"""

import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components import sensor, text_sensor, voltage_sampler
import esphome.config_validation as cv
from esphome.const import (
    CONF_ATTENUATION,
    CONF_CALIBRATION,
    CONF_DELAY,
    CONF_ENABLE_PIN,
    CONF_ID,
    CONF_INTERNAL,
    CONF_NAME,
    CONF_PIN,
    CONF_RAW,
    CONF_UPDATE_INTERVAL,
    CONF_VOLTAGE,
    CONF_WARMUP_TIME,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)
from esphome.types import ConfigType

from . import (
    ADC_LIMIT_MARGIN_V,
    CONF_A,
    CONF_ADC_ATTENUATION,
    CONF_ADC_INPUT_MAX,
    CONF_ADC_PIN_MAX,
    CONF_ADC_SAMPLES,
    CONF_AIR_REFERENCE,
    CONF_B,
    CONF_CONVERSION,
    CONF_DIVIDER,
    CONF_GAS,
    CONF_LOG_PPM,
    CONF_LOG_SENSOR,
    CONF_MAX_PPM,
    CONF_MIN_PPM,
    CONF_PERSIST,
    CONF_R0,
    CONF_R1,
    CONF_R2,
    CONF_RATIO_SENSOR,
    CONF_RL,
    CONF_RS_SENSOR,
    CONF_SAMPLE_INTERVAL,
    CONF_SAMPLES,
    CONF_VCC,
    CONF_VOLTAGE_MULTIPLIER,
    CONF_VOLTAGE_SENSOR,
    CONVERSION_MODELS,
    ESP32_ADC_INPUT_MAX_V,
    ESP32_ADC_PIN_MAX_V,
    MiCS5524GasSensor,
)
from .coefficients import (
    CONVERSION_DATASHEET,
    CONVERSION_DFROBOT,
    DEFAULT_RL_KOHM,
    SELECTIVITY_NOTE,
    gas_keys,
    label_for,
    resolve_gas,
    resolve_gas_key,
)

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["sensor", "voltage_sampler", "adc"]

ADC_ATTENUATIONS = ("0db", "2.5db", "6db", "11db", "12db", "auto")


def validate_gas(value: str) -> str:
    """Accept ``h2`` / ``H-2`` / ``hydrogen`` and return the canonical gas key."""
    key = resolve_gas_key(value)
    if key not in gas_keys():
        raise cv.Invalid(
            f"'{value}' is not a supported MiCS-5524 gas, expected one of: "
            f"{', '.join(gas_keys())}"
        )
    return key


#: Private key under which the configuration of the generated, internal ``adc``
#: sensor is kept (same trick as the sibling ``mq_gas_sensors`` component: the
#: entry has to live inside the validated config so that ESPHome's ID pass sees
#: the declared component ID).
_KEY_GENERATED_ADC = "_generated_adc"


def _build_internal_adc(config: ConfigType) -> ConfigType:
    """Validate the hidden ``adc`` entry of a ``pin:`` configuration."""
    from esphome.components.adc import sensor as adc_sensor

    adc_id = f"{config[CONF_ID].id}_adc"
    adc_config: ConfigType = {
        CONF_ID: adc_id,
        CONF_PIN: config[CONF_PIN],
        CONF_NAME: f"{adc_id} voltage",
        CONF_INTERNAL: True,
        CONF_RAW: False,
        adc_sensor.CONF_SAMPLES: config[CONF_ADC_SAMPLES],
        adc_sensor.CONF_SAMPLING_MODE: "avg",
        CONF_UPDATE_INTERVAL: "never",
    }
    if (attenuation := config.get(CONF_ADC_ATTENUATION)) is not None:
        adc_config[CONF_ATTENUATION] = attenuation

    return adc_sensor.CONFIG_SCHEMA(adc_config)


DIVIDER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_R1): cv.positive_not_null_float,
        cv.Required(CONF_R2): cv.positive_not_null_float,
    }
)


def _voltage_multiplier(config: ConfigType) -> float:
    """Inverse of the divider ratio, derived from the wiring if possible.

    ``divider: {r1: 10.0, r2: 20.0}`` (kOhm, r1 in series with the sensor output,
    r2 to ground) documents the hardware and yields ``(r1 + r2) / r2`` = 1.5 for the
    recommended 10k/20k.  ``voltage_multiplier:`` stays available as the low-level
    escape hatch; it also covers a directly connected sensor (1.0) or a sampler
    with a wider input range, e.g. an ADS1115.
    """
    divider = config.get(CONF_DIVIDER)
    if divider is not None:
        return (divider[CONF_R1] + divider[CONF_R2]) / divider[CONF_R2]
    return float(config.get(CONF_VOLTAGE_MULTIPLIER, 1.0))


CALIBRATION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DELAY, default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SAMPLES, default=10): cv.int_range(min=1, max=1000),
        cv.Optional(CONF_PERSIST, default=True): cv.boolean,
    }
)


def _validate_config(config: ConfigType) -> ConfigType:
    """Cross-check the gas/conversion/coefficient combination at compile time."""
    gas = config[CONF_GAS]
    conversion = config[CONF_CONVERSION]
    label = label_for(gas)

    try:
        resolved = resolve_gas(
            gas,
            conversion,
            a=config.get(CONF_A),
            b=config.get(CONF_B),
            min_ppm=config.get(CONF_MIN_PPM),
            max_ppm=config.get(CONF_MAX_PPM),
        )
    except ValueError as err:
        raise cv.Invalid(str(err)) from err

    if resolved.max_ppm <= resolved.min_ppm:
        raise cv.Invalid(
            f"max_ppm ({resolved.max_ppm}) must be greater than min_ppm ({resolved.min_ppm})"
        )

    if conversion == CONVERSION_DFROBOT:
        stray = [key for key in (CONF_RL, CONF_R0) if key in config]
        if stray:
            raise cv.Invalid(
                f"'conversion: {CONVERSION_DFROBOT}' derives everything from the clean-air "
                f"reference, remove {', '.join(f'{key}:' for key in stray)} "
                f"(or use 'conversion: {CONVERSION_DATASHEET}')"
            )
    elif CONF_AIR_REFERENCE in config:
        raise cv.Invalid(
            f"'air_reference:' belongs to 'conversion: {CONVERSION_DFROBOT}', "
            f"use 'r0:' with 'conversion: {CONVERSION_DATASHEET}'"
        )

    fixed = CONF_AIR_REFERENCE if conversion == CONVERSION_DFROBOT else CONF_R0
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
        "%s: conversion '%s' - %s (treat the value as an estimate)",
        label,
        conversion,
        SELECTIVITY_NOTE,
    )

    if CONF_DIVIDER in config and CONF_VOLTAGE_MULTIPLIER in config:
        raise cv.Invalid(
            "use either 'divider:' or 'voltage_multiplier:', not both - "
            "'divider:' already derives the multiplier from r1/r2"
        )

    # ADC range guard: the module output can reach VCC and the divider maps that
    # onto the ADC pin.  Only meaningful when this component owns the pin, or when
    # the limits were declared for an external sampler (e.g. an ADS1115).
    if CONF_PIN in config or CONF_ADC_INPUT_MAX in config or CONF_ADC_PIN_MAX in config:
        multiplier = _voltage_multiplier(config)
        input_max = float(config.get(CONF_ADC_INPUT_MAX, ESP32_ADC_INPUT_MAX_V))
        pin_max = float(config.get(CONF_ADC_PIN_MAX, ESP32_ADC_PIN_MAX_V))
        vcc = float(config[CONF_VCC])
        expected_max = vcc / multiplier
        if expected_max > pin_max + ADC_LIMIT_MARGIN_V:
            raise cv.Invalid(
                f"the module output can reach {vcc:.2f} V and the configured divider "
                f"maps that to {expected_max:.2f} V on the ADC pin, above 'adc_pin_max' "
                f"({pin_max:.2f} V) - that can damage the pin. Use "
                f"'divider: {{r1: 10.0, r2: 20.0}}' (-> 1.5) or "
                f"'{{r1: 10.0, r2: 10.0}}' (-> 2.0), or set 'adc_pin_max' to your "
                f"sampler's limit (6.144 for an ADS1115)"
            )
        if expected_max > input_max + ADC_LIMIT_MARGIN_V:
            _LOGGER.warning(
                "%s: the module output can reach %.2f V and the configured divider maps "
                "that to %.2f V at the ADC input, above 'adc_input_max' (%.2f V): the top "
                "of the range clips or reads non-linearly. 10k/20k gives 3.33 V - raise "
                "'adc_input_max' if that is intentional",
                label,
                vcc,
                expected_max,
                input_max,
            )

    if CONF_PIN in config:
        config[_KEY_GENERATED_ADC] = _build_internal_adc(config)

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
            cv.Optional(CONF_CONVERSION, default=CONVERSION_DFROBOT): cv.one_of(
                *CONVERSION_MODELS, lower=True
            ),
            cv.Optional(CONF_VOLTAGE): cv.use_id(voltage_sampler.VoltageSampler),
            cv.Optional(CONF_PIN): cv.valid,
            cv.SplitDefault(CONF_ADC_ATTENUATION, esp32="12db"): cv.All(
                cv.only_on_esp32, cv.one_of(*ADC_ATTENUATIONS, lower=True)
            ),
            cv.Optional(CONF_ADC_SAMPLES, default=1): cv.int_range(min=1, max=255),
            cv.Optional(CONF_VOLTAGE_MULTIPLIER): cv.positive_not_null_float,
            cv.Optional(CONF_DIVIDER): DIVIDER_SCHEMA,
            cv.Optional(CONF_ADC_INPUT_MAX): cv.positive_not_null_float,
            cv.Optional(CONF_ADC_PIN_MAX): cv.positive_not_null_float,
            cv.Optional(CONF_VCC, default=5.0): cv.positive_not_null_float,
            cv.Optional(CONF_RL): cv.positive_not_null_float,
            cv.Optional(CONF_R0): cv.positive_not_null_float,
            cv.Optional(CONF_AIR_REFERENCE): cv.positive_not_null_float,
            cv.Optional(CONF_A): cv.float_,
            cv.Optional(CONF_B): cv.float_,
            cv.Optional(CONF_SAMPLES, default=4): cv.int_range(min=1, max=255),
            cv.Optional(
                CONF_SAMPLE_INTERVAL, default="20ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_WARMUP_TIME, default="3min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MIN_PPM): cv.float_,
            cv.Optional(CONF_MAX_PPM): cv.positive_not_null_float,
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_CALIBRATION): CALIBRATION_SCHEMA,
            cv.Optional(CONF_RATIO_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_RS_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_VOLTAGE_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_LOG_SENSOR): cv.use_id(text_sensor.TextSensor),
            cv.Optional(CONF_LOG_PPM, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("30s")),
    cv.has_exactly_one_key(CONF_VOLTAGE, CONF_PIN),
    _validate_config,
)


async def _create_internal_adc(config: ConfigType):
    """Instantiate the hidden, internal ``adc`` sensor used by ``pin:`` configurations.

    The entry was validated by :func:`_build_internal_adc`; here the ADC platform's
    own ``to_code`` is reused so attenuation, sampling count and platform specifics
    behave exactly like a hand written ``sensor: - platform: adc`` entry.
    """
    from esphome.components.adc import sensor as adc_sensor

    adc_config = config.get(_KEY_GENERATED_ADC)
    if adc_config is None:  # pragma: no cover - would be a bug in this component
        raise ValueError(
            f"internal adc sensor for '{config[CONF_ID].id}' was not generated, "
            "the 'pin:' configuration could not be validated"
        )

    await adc_sensor.to_code(adc_config)
    return await cg.get_variable(adc_config[CONF_ID])


async def to_code(config: ConfigType) -> None:
    resolved = resolve_gas(
        config[CONF_GAS],
        config[CONF_CONVERSION],
        a=config.get(CONF_A),
        b=config.get(CONF_B),
        min_ppm=config.get(CONF_MIN_PPM),
        max_ppm=config.get(CONF_MAX_PPM),
    )

    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    cg.add(var.set_gas(resolved.label))
    cg.add(var.set_conversion(CONVERSION_MODELS[resolved.conversion]))
    cg.add(var.set_threshold(resolved.threshold))
    cg.add(var.set_gain(resolved.gain))
    cg.add(var.set_vendor_min_ppm(resolved.vendor_min_ppm))
    cg.add(var.set_vendor_max_ppm(resolved.vendor_max_ppm))
    cg.add(var.set_a(resolved.a))
    cg.add(var.set_b(resolved.b))
    cg.add(var.set_min_ppm(resolved.min_ppm))
    cg.add(var.set_max_ppm(resolved.max_ppm))
    rl = float(config.get(CONF_RL, DEFAULT_RL_KOHM))
    cg.add(var.set_rl(rl))
    cg.add(var.set_vcc(config[CONF_VCC]))
    cg.add(var.set_voltage_multiplier(_voltage_multiplier(config)))
    cg.add(var.set_samples(config[CONF_SAMPLES]))
    cg.add(var.set_sample_interval(config[CONF_SAMPLE_INTERVAL]))
    cg.add(var.set_warmup_time(config[CONF_WARMUP_TIME]))

    if (r0 := config.get(CONF_R0)) is not None:
        cg.add(var.set_r0(r0))
    if (air_reference := config.get(CONF_AIR_REFERENCE)) is not None:
        cg.add(var.set_air_reference(air_reference))

    fixed = CONF_AIR_REFERENCE if resolved.conversion == CONVERSION_DFROBOT else CONF_R0
    calibration = config.get(CONF_CALIBRATION)
    if calibration is not None and fixed not in config:
        cg.add(
            var.set_calibration(
                True,
                calibration[CONF_DELAY],
                calibration[CONF_SAMPLES],
                calibration[CONF_PERSIST],
            )
        )

    if (enable := config.get(CONF_ENABLE_PIN)) is not None:
        cg.add(var.set_enable_pin(await cg.gpio_pin_expression(enable)))

    if source_id := config.get(CONF_VOLTAGE):
        cg.add(var.set_source(await cg.get_variable(source_id)))
    else:
        cg.add(var.set_source(await _create_internal_adc(config)))

    for key, setter in (
        (CONF_RATIO_SENSOR, var.set_ratio_sensor),
        (CONF_RS_SENSOR, var.set_rs_sensor),
        (CONF_VOLTAGE_SENSOR, var.set_voltage_sensor),
        (CONF_LOG_SENSOR, var.set_log_sensor),
    ):
        if (target := config.get(key)) is not None:
            cg.add(setter(await cg.get_variable(target)))

    cg.add(var.set_log_ppm(config[CONF_LOG_PPM]))

    _LOGGER.debug(
        "%s: conversion=%s threshold=%s gain=%s a=%s b=%s rl=%s vcc=%s range=%s..%s ppm",
        resolved.label,
        resolved.conversion,
        resolved.threshold,
        resolved.gain,
        resolved.a,
        resolved.b,
        rl,
        config[CONF_VCC],
        resolved.min_ppm,
        resolved.max_ppm,
    )
