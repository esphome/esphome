"""``mics_5524_gas_sensor`` - ESPHome sensor platform for the MiCS-5524.

One entry configures one physical sensor: the analog output is read and converted once
per ``update_interval``, and every gas listed under it is published as its own
sub-sensor.  The MiCS-5524 responds to several gases at once, so the readings report the
same mixture of CO, H2, C2H5OH, NH3 and CH4 with different calibrations, and none of
them can say which of the gases is present.

The signal comes from an existing voltage sampler (``voltage:``, e.g. an ``adc`` sensor
configured next to this one) or from a pin this component owns (``pin:``, which creates
an internal ``adc`` sensor for you).
"""

import logging

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import sensor, voltage_sampler
import esphome.config_validation as cv
from esphome.const import (
    CONF_ATTENUATION,
    CONF_CALIBRATION,
    CONF_DELAY,
    CONF_ENABLE_PIN,
    CONF_ID,
    CONF_INTERNAL,
    CONF_INVERTED,
    CONF_NAME,
    CONF_PIN,
    CONF_RAW,
    CONF_UPDATE_INTERVAL,
    CONF_VOLTAGE,
    CONF_VOLTAGE_DIVIDER,
    CONF_WARMUP_TIME,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
    UNIT_VOLT,
)
from esphome.core import CORE
from esphome.types import ConfigType

from . import (
    ADC_LIMIT_MARGIN_V,
    CONF_ADC_ATTENUATION,
    CONF_ADC_INPUT_MAX,
    CONF_ADC_PIN_MAX,
    CONF_ADC_SAMPLES,
    CONF_AIR_REFERENCE,
    CONF_COEFFICIENT_A,
    CONF_COEFFICIENT_B,
    CONF_CONVERSION,
    CONF_MAX_PPM,
    CONF_MIN_PPM,
    CONF_PERSIST,
    CONF_R0,
    CONF_R1,
    CONF_R2,
    CONF_RATIO_SENSOR,
    CONF_RL,
    CONF_RS_SENSOR,
    CONF_SAMPLE_COUNT,
    CONF_SAMPLE_INTERVAL,
    CONF_VCC,
    CONF_VOLTAGE_MULTIPLIER,
    CONF_VOLTAGE_SENSOR,
    CONVERSION_MODELS,
    ESP32_ADC_INPUT_MAX_V,
    ESP32_ADC_PIN_MAX_V,
    MiCS5524Component,
    MiCS5524GasSensor,
)
from .coefficients import (
    CONVERSION_DATASHEET,
    CONVERSION_DFROBOT,
    CONVERSIONS,
    DEFAULT_RL_KOHM,
    GASES,
    ResolvedGas,
    resolve_gas,
)

_LOGGER = logging.getLogger(__name__)


def _auto_load(config: ConfigType | None) -> list[str]:
    """Components this platform needs: ``adc`` only for a ``pin:`` source.

    A ``pin:`` configuration creates its own internal ``adc`` sensor, so the
    ``adc`` component has to be part of the build.  A ``voltage:``
    configuration reads an existing sampler and must not pull ``adc`` in: it
    has no host implementation, so it breaks the C++ unit-test link, and it
    bloats firmware that never uses it.
    """
    if config is not None and CONF_PIN in config:
        return ["sensor", "voltage_sampler", "adc"]
    return ["sensor", "voltage_sampler"]


AUTO_LOAD = _auto_load

#: The main loop runs every ~16 ms, so a shorter spacing cannot sample more often
#: than that and only makes a calibration overrun a tick of everything else.
MIN_SAMPLE_INTERVAL_MS = 16

#: At most this many conversions of the analog output per reading: they run back to
#: back inside one loop pass, so a longer burst delays everything else.
MAX_SAMPLE_COUNT = 16

ADC_ATTENUATIONS = ("0db", "2.5db", "6db", "11db", "12db", "auto")

#: One slot per configured gas, sizing the gas list of each sensor.
_request_gas_slot = cg.slot_counter("MICS_5524_GAS_COUNT")

#: Private key holding the generated, internal ``adc`` sensor configuration; the entry
#: lives inside the validated config so ESPHome's ID pass sees its ID.
_KEY_GENERATED_ADC = "_generated_adc"

DIVIDER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_R1): cv.positive_not_null_float,
        cv.Required(CONF_R2): cv.positive_not_null_float,
    }
)


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
    elif CORE.is_esp32:
        # The ESP32 ADC defaults to 0 dB (~1 V full scale), while the divider
        # guard allows up to 3.3 V: without an explicit attenuation the pin would
        # clip and the calibrated reference would be wrong without a log.
        adc_config[CONF_ATTENUATION] = "12db"

    return adc_sensor.CONFIG_SCHEMA(adc_config)


def _voltage_multiplier(config: ConfigType) -> float:
    """Volts of the analog output per volt at the sampler input.

    ``voltage_divider:`` is the wiring in front of the sampler (r1 from the sensor to the
    pin, r2 from the pin to ground); ``voltage_multiplier:`` is the low-level equivalent,
    1.0 for a directly connected sensor and more for a sampler with a wider input range
    (e.g. an ADS1115).
    """
    if (divider := config.get(CONF_VOLTAGE_DIVIDER)) is not None:
        return (divider[CONF_R1] + divider[CONF_R2]) / divider[CONF_R2]
    return float(config.get(CONF_VOLTAGE_MULTIPLIER, 1.0))


def _reject_divider_and_multiplier(config: ConfigType) -> ConfigType:
    """Reject a `voltage_divider:` together with an explicit `voltage_multiplier:`.

    Runs before the schema, so the schema default of `voltage_multiplier:` is not
    mistaken for a configured value.
    """
    if CONF_VOLTAGE_DIVIDER in config and CONF_VOLTAGE_MULTIPLIER in config:
        raise cv.Invalid(
            "use either 'voltage_divider:' or 'voltage_multiplier:', not both - "
            "'voltage_divider:' already derives the multiplier from r1/r2"
        )
    return config


def _validate_divider(config: ConfigType) -> ConfigType:
    """Reject a divider that can push the pin past what the sampler survives.

    The pin sees the supply across r1/r2, which is the largest voltage the sensor can
    ever produce, so the check holds for every reading.  The limits are the classic
    ESP32 ADC ones unless the configuration declares the ones of its own sampler.
    """
    if (divider := config.get(CONF_VOLTAGE_DIVIDER)) is None:
        return config

    supply = config[CONF_VCC]
    pin_voltage = supply * divider[CONF_R2] / (divider[CONF_R1] + divider[CONF_R2])
    if pin_voltage > config[CONF_ADC_PIN_MAX] + ADC_LIMIT_MARGIN_V:
        raise cv.Invalid(
            f"the divider brings {supply:g} V down to {pin_voltage:.4g} V on the pin, "
            f"above the {config[CONF_ADC_PIN_MAX]:g} V the ADC tolerates - raise r1/r2, or "
            "declare 'adc_pin_max:' for a sampler that takes more"
        )
    if pin_voltage > config[CONF_ADC_INPUT_MAX] + ADC_LIMIT_MARGIN_V:
        raise cv.Invalid(
            f"the divider brings {supply:g} V down to {pin_voltage:.4g} V on the pin, "
            f"above the {config[CONF_ADC_INPUT_MAX]:g} V measuring range of the sampler - "
            "raise r1/r2, or declare 'adc_input_max:' for a sampler with a wider range"
        )
    return config


def _gas_schema(gas_key: str) -> cv.Schema:
    """Sub-sensor schema of one gas: a PPM sensor with its own range and coefficients."""
    return sensor.sensor_schema(
        MiCS5524GasSensor,
        unit_of_measurement=UNIT_PARTS_PER_MILLION,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:molecule",
    ).extend(
        {
            # No default here: the vendor range and the datasheet fit have different
            # ceilings, so the default is resolved per conversion model in
            # `resolve_gas` instead of leaking one model's range into the other.
            cv.Optional(CONF_MAX_PPM): cv.positive_not_null_float,
            cv.Optional(CONF_MIN_PPM): cv.positive_not_null_float,
            cv.Inclusive(
                CONF_COEFFICIENT_A, "coefficients"
            ): cv.positive_not_null_float,
            cv.Inclusive(CONF_COEFFICIENT_B, "coefficients"): cv.float_,
        }
    )


CALIBRATION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DELAY, default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SAMPLE_COUNT, default=10): cv.int_range(
            min=2, max=MAX_SAMPLE_COUNT
        ),
        cv.Optional(CONF_SAMPLE_INTERVAL, default="20ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=MIN_SAMPLE_INTERVAL_MS)),
        ),
        cv.Optional(CONF_PERSIST, default=True): cv.boolean,
    }
)


def _resolve_gas(gas_key: str, config: ConfigType) -> ResolvedGas:
    """Resolve one configured gas against the conversion model of its hub."""
    gas = config[gas_key]
    return resolve_gas(
        gas_key,
        config[CONF_CONVERSION],
        a=gas.get(CONF_COEFFICIENT_A),
        b=gas.get(CONF_COEFFICIENT_B),
        min_ppm=gas.get(CONF_MIN_PPM),
        max_ppm=gas.get(CONF_MAX_PPM),
    )


def _validate_config(config: ConfigType) -> ConfigType:
    """Check the reference, the conversion and every gas against each other.

    Everything here is a compile-time error rather than a runtime surprise: the
    reference keys belong to one model, `max_ppm:` cannot ask for a value the active
    model can never reach, and the datasheet model needs coefficients for every gas it
    is used with.
    """
    conversion = config[CONF_CONVERSION]
    vendor_model = conversion == CONVERSION_DFROBOT

    if (air_reference := config.get(CONF_AIR_REFERENCE)) is not None:
        if not vendor_model:
            raise cv.Invalid(
                f"'air_reference:' belongs to 'conversion: {CONVERSION_DFROBOT}', use "
                f"'r0:' with 'conversion: {CONVERSION_DATASHEET}'"
            )
        # The reference is `x_air = vcc - V_AO` as read in clean air, so it is always
        # below the supply: a larger one would report every gas as 0 ppm forever.
        if air_reference >= config[CONF_VCC]:
            raise cv.Invalid(
                f"air_reference ({air_reference:g} V) must be below vcc "
                f"({config[CONF_VCC]:g} V): the reference is how far the analog "
                "output sits below the supply in clean air, so a larger value cannot "
                "come from a reading"
            )
    elif CONF_CALIBRATION not in config and CONF_R0 not in config:
        # The datasheet model takes its reference as `r0:`, so the vendor key is only one
        # of the two ways to configure one.
        _LOGGER.warning(
            "no clean-air reference configured - set '%s' or 'calibration:', or every "
            "PPM value stays unknown until the sensor has been calibrated in clean air",
            CONF_AIR_REFERENCE if vendor_model else CONF_R0,
        )

    if CONF_R0 in config and vendor_model:
        raise cv.Invalid(
            f"'r0:' belongs to 'conversion: {CONVERSION_DATASHEET}', use "
            f"'air_reference:' with 'conversion: {CONVERSION_DFROBOT}'"
        )
    if CONF_RL in config and vendor_model:
        raise cv.Invalid(
            f"'rl:' belongs to 'conversion: {CONVERSION_DATASHEET}' - "
            f"'conversion: {CONVERSION_DFROBOT}' derives everything from the clean-air "
            "reference"
        )
    if CONF_RS_SENSOR in config and vendor_model:
        raise cv.Invalid(
            f"'rs_sensor:' belongs to 'conversion: {CONVERSION_DATASHEET}' - "
            "the vendor model never computes RS, so the sensor would stay unknown"
        )

    for gas_key in GASES:
        if gas_key not in config:
            continue
        try:
            resolved = _resolve_gas(gas_key, config)
        except ValueError as err:
            raise cv.Invalid(str(err)) from err
        if resolved.max_ppm <= resolved.min_ppm:
            raise cv.Invalid(
                f"max_ppm ({resolved.max_ppm:g}) must be greater than min_ppm "
                f"({resolved.min_ppm:g})"
            )
        # The vendor model has a documented measuring range; the datasheet fit has none,
        # so only the vendor model can be checked.
        if vendor_model and resolved.max_ppm > resolved.vendor_max_ppm:
            raise cv.Invalid(
                f"max_ppm ({resolved.max_ppm:g}) is above the {resolved.label} measuring "
                f"range of {resolved.vendor_max_ppm:g} ppm, so the conversion can never "
                "reach it"
            )
    return config


def _validate_enable_pin(config: ConfigType) -> ConfigType:
    """Warn when the EN pin is left active high, which keeps the module asleep.

    The component powers the module by driving the pin active, and the EN input of the
    DFRobot breakout is active low, so a pin without ``inverted: true`` is driven high
    and the analog output stays at 0 V.
    """
    if (enable := config.get(CONF_ENABLE_PIN)) is not None and not enable.get(
        CONF_INVERTED, False
    ):
        _LOGGER.warning(
            "'enable_pin:' is driven active high, but the EN input of the DFRobot "
            "MiCS-5524 breakout is active low: without 'inverted: true' the module stays "
            "asleep and every reading stays at 0 V"
        )
    return config


def _build_pin_source(config: ConfigType) -> ConfigType:
    """Validate the hidden ``adc`` entry of a ``pin:`` configuration.

    The entry is built here, inside the validated config, so ESPHome's ID pass sees the
    ID of the generated sensor and a wrong ADC configuration fails the build instead of
    the compiler.
    """
    if CONF_PIN in config:
        config[_KEY_GENERATED_ADC] = _build_internal_adc(config)
    return config


CONFIG_SCHEMA = cv.All(
    _reject_divider_and_multiplier,
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MiCS5524Component),
            cv.Optional(CONF_VOLTAGE): cv.use_id(voltage_sampler.VoltageSampler),
            # The pin of a `pin:` configuration is validated with the hidden `adc` entry
            # built below, which owns the rules for a pin the ADC can read.
            cv.Optional(CONF_PIN): cv.valid,
            cv.Optional(CONF_VOLTAGE_DIVIDER): DIVIDER_SCHEMA,
            cv.Optional(
                CONF_ADC_INPUT_MAX, default=ESP32_ADC_INPUT_MAX_V
            ): cv.positive_not_null_float,
            cv.Optional(
                CONF_ADC_PIN_MAX, default=ESP32_ADC_PIN_MAX_V
            ): cv.positive_not_null_float,
            cv.Optional(CONF_ADC_ATTENUATION): cv.one_of(*ADC_ATTENUATIONS, lower=True),
            cv.Optional(CONF_ADC_SAMPLES, default=2): cv.int_range(
                min=1, max=MAX_SAMPLE_COUNT
            ),
            cv.Optional(CONF_CONVERSION, default=CONVERSION_DFROBOT): cv.one_of(
                *CONVERSIONS, lower=True
            ),
            cv.Optional(
                CONF_VOLTAGE_MULTIPLIER, default=1.0
            ): cv.positive_not_null_float,
            cv.Optional(CONF_VCC, default=5.0): cv.positive_not_null_float,
            cv.Optional(CONF_RL): cv.positive_not_null_float,
            cv.Optional(CONF_AIR_REFERENCE): cv.positive_not_null_float,
            cv.Optional(CONF_R0): cv.positive_not_null_float,
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_SAMPLE_COUNT, default=4): cv.int_range(
                min=1, max=MAX_SAMPLE_COUNT
            ),
            cv.Optional(
                CONF_WARMUP_TIME, default="3min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_CALIBRATION): CALIBRATION_SCHEMA,
            # Readings the conversion is built from: declared here like the gases, so the
            # component owns them instead of pointing at sensors declared elsewhere.
            cv.Optional(CONF_VOLTAGE_SENSOR): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_VOLTAGE,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_RATIO_SENSOR): sensor.sensor_schema(
                accuracy_decimals=4,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_RS_SENSOR): sensor.sensor_schema(
                unit_of_measurement="kΩ",
                accuracy_decimals=2,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            **{cv.Optional(gas_key): _gas_schema(gas_key) for gas_key in GASES},
        }
    ).extend(cv.polling_component_schema("30s")),
    cv.has_exactly_one_key(CONF_VOLTAGE, CONF_PIN),
    cv.has_at_least_one_key(*GASES),
    cv.has_at_most_one_key(CONF_AIR_REFERENCE, CONF_R0, CONF_CALIBRATION),
    _validate_divider,
    _validate_config,
    _validate_enable_pin,
    _build_pin_source,
)


async def to_code(config: ConfigType) -> None:
    # The sampler is a constructor argument: no reading is possible without it, so the
    # component has no valid state in which it is missing.  A `pin:` configuration
    # creates its own internal `adc` sensor here and hands that one over instead.
    source = (
        await cg.get_variable(config[CONF_VOLTAGE])
        if CONF_VOLTAGE in config
        else await _create_internal_adc(config)
    )
    var = cg.new_Pvariable(config[CONF_ID], source)
    await cg.register_component(var, config)

    cg.add(var.set_conversion(CONVERSION_MODELS[config[CONF_CONVERSION]]))
    cg.add(var.set_vcc(config[CONF_VCC]))
    cg.add(var.set_rl(float(config.get(CONF_RL, DEFAULT_RL_KOHM))))
    cg.add(var.set_voltage_multiplier(_voltage_multiplier(config)))
    cg.add(var.set_samples(config[CONF_SAMPLE_COUNT]))
    cg.add(var.set_warmup_time(config[CONF_WARMUP_TIME]))

    if (enable := config.get(CONF_ENABLE_PIN)) is not None:
        cg.add(var.set_enable_pin(await cg.gpio_pin_expression(enable)))
    if (air_reference := config.get(CONF_AIR_REFERENCE)) is not None:
        cg.add(var.set_air_reference(air_reference))
    elif (r0 := config.get(CONF_R0)) is not None:
        cg.add(var.set_r0(r0))
    elif (calibration := config.get(CONF_CALIBRATION)) is not None:
        cg.add(
            var.set_calibration(
                calibration[CONF_DELAY],
                calibration[CONF_SAMPLE_COUNT],
                calibration[CONF_SAMPLE_INTERVAL],
                calibration[CONF_PERSIST],
            )
        )

    # The diagnostics report the values every gas is converted from, so they are declared
    # here like the gases and not pointed at from outside.
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_VOLTAGE_SENSOR, var.set_voltage_sensor)
    await sensors(CONF_RATIO_SENSOR, var.set_ratio_sensor)
    await sensors(CONF_RS_SENSOR, var.set_rs_sensor)

    for gas_key in GASES:
        if gas_key not in config:
            continue
        resolved = _resolve_gas(gas_key, config)
        _request_gas_slot(str(var))
        await sensors(
            gas_key,
            var.register_gas_sensor,
            resolved.label,
            CONVERSION_MODELS[resolved.conversion],
            resolved.threshold,
            resolved.gain,
            resolved.a,
            resolved.b,
            resolved.min_ppm,
            resolved.max_ppm,
        )
        _LOGGER.debug(
            "%s: conversion=%s threshold=%s gain=%s a=%s b=%s range=%s..%s ppm",
            resolved.label,
            resolved.conversion,
            resolved.threshold,
            resolved.gain,
            resolved.a,
            resolved.b,
            resolved.min_ppm,
            resolved.max_ppm,
        )


async def _create_internal_adc(config: ConfigType) -> cg.MockObj:
    """Create the ``adc`` sensor of a ``pin:`` configuration and return it."""
    from esphome.components.adc import sensor as adc_sensor

    adc_config = config.get(_KEY_GENERATED_ADC)
    if adc_config is None:
        raise ValueError(
            f"internal adc sensor for '{config[CONF_ID].id}' was not generated, "
            "the 'pin:' configuration could not be validated"
        )

    await adc_sensor.to_code(adc_config)
    return await cg.get_variable(adc_config[CONF_ID])


CALIBRATION_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(MiCS5524Component),
    }
)

automation.register_apply_action(
    "mics_5524_gas_sensor.request_calibration",
    CALIBRATION_ACTION_SCHEMA,
    automation.ApplyCall("request_calibration()"),
)
