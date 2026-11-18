"""MiCS-5524 sensor platform: one entry per module, one sub-sensor per configured gas."""

import logging

from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import sensor, voltage_sampler
from esphome.components.adc import ATTENUATION_MODES
from esphome.components.esp32 import get_esp32_variant
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
from esphome.helpers import fnv1_hash
from esphome.types import ConfigType

from . import (
    ADC_LIMIT_MARGIN_V,
    ADC_LIMITS_V,
    CONF_ADC_ATTENUATION,
    CONF_ADC_INPUT_MAX,
    CONF_ADC_PIN_MAX,
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
    MiCS5524Component,
    MiCS5524GasSensor,
    esp32_attenuation_input_max,
)
from .coefficients import (
    CONVERSION_DATASHEET,
    CONVERSION_DFROBOT,
    GASES,
    ResolvedGas,
    resolve_gas,
)

_LOGGER = logging.getLogger(__name__)


def _auto_load(config: ConfigType | None) -> list[str]:
    """Load adc only for a pin: source; it has no host build, so the unit tests cannot link it."""
    if config is not None and CONF_PIN in config:
        return ["sensor", "voltage_sampler", "adc"]
    return ["sensor", "voltage_sampler"]


AUTO_LOAD = _auto_load

# The main loop runs every ~16 ms, so a shorter spacing cannot sample more often.
MIN_SAMPLE_INTERVAL_MS = 16

# Conversions per reading run back to back inside one loop pass.
MAX_SAMPLE_COUNT = 16

_request_gas_slot = cg.slot_counter("MICS_5524_GAS_COUNT")

# The generated adc entry lives in the validated config so the ID pass sees its ID.
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
        # sample_count of the component is the only averaging layer.
        adc_sensor.CONF_SAMPLES: 1,
        adc_sensor.CONF_SAMPLING_MODE: "avg",
        CONF_UPDATE_INTERVAL: "never",
    }
    if (attenuation := config.get(CONF_ADC_ATTENUATION)) is not None:
        adc_config[CONF_ATTENUATION] = attenuation
    elif CORE.is_esp32:
        # The 0 dB default (~1 V) would clip the 3.3 V the divider guard allows.
        adc_config[CONF_ATTENUATION] = "12db"

    return adc_sensor.CONFIG_SCHEMA(adc_config)


def _voltage_multiplier(config: ConfigType) -> float:
    """Output volts per sampler volt: from voltage_divider: when set, else voltage_multiplier:."""
    if (divider := config.get(CONF_VOLTAGE_DIVIDER)) is not None:
        return (divider[CONF_R1] + divider[CONF_R2]) / divider[CONF_R2]
    return float(config.get(CONF_VOLTAGE_MULTIPLIER, 1.0))


def _preference_key(config: ConfigType) -> int:
    """Flash key of the stored reference; a new vcc, scaling or rl must not load an old value."""
    parts = [
        config[CONF_ID].id,
        f"{config[CONF_VCC]:g}",
        f"{_voltage_multiplier(config):g}",
    ]
    if (rl := config.get(CONF_RL)) is not None:
        parts.append(f"{rl:g}")
    return fnv1_hash(":".join(parts))


def _validate_adc_range(config: ConfigType) -> ConfigType:
    """Reject wiring that puts the supply, the largest possible reading, past the sampler limits.

    A ``pin:`` source is always checked; a ``voltage:`` source only through its ``voltage_divider:``,
    since the scaling of an external sampler is not known here.
    """
    divider = config.get(CONF_VOLTAGE_DIVIDER)
    if divider is None and CONF_PIN not in config:
        return config

    supply = config[CONF_VCC]
    pin_voltage = supply / _voltage_multiplier(config)
    if (limits := ADC_LIMITS_V.get(CORE.target_platform)) is None:
        if CONF_ADC_INPUT_MAX not in config or CONF_ADC_PIN_MAX not in config:
            raise cv.Invalid(
                f"no built-in ADC limits for {CORE.target_platform}: declare "
                "'adc_input_max:' and 'adc_pin_max:' so the wiring can be checked"
            )
        limits = (config[CONF_ADC_INPUT_MAX], config[CONF_ADC_PIN_MAX])
    default_input_max, default_pin_max = limits
    if CONF_PIN in config and CORE.is_esp32:
        # The attenuation picks the measuring range of the internal ADC
        default_input_max = esp32_attenuation_input_max(
            get_esp32_variant(), config.get(CONF_ADC_ATTENUATION, "12db")
        )
    pin_max = config.get(CONF_ADC_PIN_MAX, default_pin_max)
    input_max = config.get(CONF_ADC_INPUT_MAX, default_input_max)
    if divider is not None:
        wiring = (
            f"the divider brings {supply:g} V down to {pin_voltage:.4g} V on the pin"
        )
        remedy = "raise r1/r2"
    elif CONF_VOLTAGE_MULTIPLIER in config:
        wiring = f"{supply:g} V scaled by 1/{config[CONF_VOLTAGE_MULTIPLIER]:g} is {pin_voltage:.4g} V on the pin"
        remedy = "raise 'voltage_multiplier:' to match the wiring"
    else:
        wiring = f"the {supply:g} V supply reaches the pin unscaled"
        remedy = (
            "add a 'voltage_divider:' (or 'voltage_multiplier:' for one already wired)"
        )
    if pin_voltage > pin_max + ADC_LIMIT_MARGIN_V:
        raise cv.Invalid(
            f"{wiring}, above the {pin_max:g} V the ADC tolerates - {remedy}, or "
            "declare 'adc_pin_max:' for a sampler that takes more"
        )
    if pin_voltage > input_max + ADC_LIMIT_MARGIN_V:
        raise cv.Invalid(
            f"{wiring}, above the {input_max:g} V measuring range of the sampler - {remedy}, "
            "or declare 'adc_input_max:' for a sampler with a wider range"
        )
    return config


def _gas_schema(gas_key: str) -> cv.Schema:
    """Schema of one gas sub-sensor."""
    return sensor.sensor_schema(
        MiCS5524GasSensor,
        unit_of_measurement=UNIT_PARTS_PER_MILLION,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:molecule",
    ).extend(
        {
            # No default: each conversion model has its own ceiling, resolved in resolve_gas.
            cv.Optional(CONF_MAX_PPM): cv.positive_not_null_float,
            cv.Optional(CONF_MIN_PPM): cv.positive_float,
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
    """Check the reference keys, the conversion and every gas against each other."""
    conversion = config[CONF_CONVERSION]
    vendor_model = conversion == CONVERSION_DFROBOT

    if (air_reference := config.get(CONF_AIR_REFERENCE)) is not None:
        if not vendor_model:
            raise cv.Invalid(
                f"'air_reference:' belongs to 'conversion: {CONVERSION_DFROBOT}', use "
                f"'r0:' with 'conversion: {CONVERSION_DATASHEET}'"
            )
        # The reference is vcc - V_AO in clean air, so a larger one reads 0 ppm forever.
        if air_reference >= config[CONF_VCC]:
            raise cv.Invalid(
                f"air_reference ({air_reference:g} V) must be below vcc "
                f"({config[CONF_VCC]:g} V): it is how far the analog output sits "
                "below the supply in clean air"
            )
    elif CONF_CALIBRATION not in config and CONF_R0 not in config:
        _LOGGER.warning(
            "no clean-air reference configured - set '%s' or 'calibration:', or every "
            "PPM value stays unknown until the sensor has been calibrated in clean air",
            CONF_AIR_REFERENCE if vendor_model else CONF_R0,
        )

    if vendor_model:
        for key, reason in (
            (CONF_R0, f"use 'air_reference:' with 'conversion: {CONVERSION_DFROBOT}'"),
            (
                CONF_RL,
                (
                    f"'conversion: {CONVERSION_DFROBOT}' derives everything from the "
                    "clean-air reference"
                ),
            ),
            (
                CONF_RS_SENSOR,
                "the vendor model never computes RS, so the sensor would stay unknown",
            ),
        ):
            if key in config:
                raise cv.Invalid(
                    f"'{key}:' belongs to 'conversion: {CONVERSION_DATASHEET}' - {reason}"
                )

    for gas_key, definition in GASES.items():
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
        # Only the vendor model has a documented range to check against.
        if vendor_model and resolved.max_ppm > (
            vendor_max_ppm := definition.vendor.max_ppm
        ):
            raise cv.Invalid(
                f"max_ppm ({resolved.max_ppm:g}) is above the {resolved.label} measuring "
                f"range of {vendor_max_ppm:g} ppm, so the conversion can never reach it"
            )
    return config


def _validate_enable_pin(config: ConfigType) -> ConfigType:
    """Warn about an EN pin without inverted: true, which keeps the active-low module asleep."""
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
    """Build the hidden adc entry inside the validated config so the ID pass sees its ID."""
    if CONF_PIN in config:
        config[_KEY_GENERATED_ADC] = _build_internal_adc(config)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MiCS5524Component),
            cv.Optional(CONF_VOLTAGE): cv.use_id(voltage_sampler.VoltageSampler),
            # Validated by the hidden adc entry built below.
            cv.Optional(CONF_PIN): cv.valid,
            cv.Optional(CONF_VOLTAGE_DIVIDER): DIVIDER_SCHEMA,
            cv.Optional(CONF_ADC_INPUT_MAX): cv.positive_not_null_float,
            cv.Optional(CONF_ADC_PIN_MAX): cv.positive_not_null_float,
            cv.Optional(CONF_ADC_ATTENUATION): cv.one_of(
                *ATTENUATION_MODES, lower=True
            ),
            cv.Optional(CONF_CONVERSION, default=CONVERSION_DFROBOT): cv.enum(
                CONVERSION_MODELS, lower=True
            ),
            cv.Optional(CONF_VOLTAGE_MULTIPLIER): cv.positive_not_null_float,
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
    cv.has_at_most_one_key(CONF_VOLTAGE_DIVIDER, CONF_VOLTAGE_MULTIPLIER),
    cv.has_at_least_one_key(*GASES),
    cv.has_at_most_one_key(CONF_AIR_REFERENCE, CONF_R0, CONF_CALIBRATION),
    cv.key_requires(
        (CONF_ADC_INPUT_MAX, CONF_ADC_PIN_MAX),
        CONF_VOLTAGE_DIVIDER,
        CONF_PIN,
        any_of=True,
    ),
    cv.key_requires(
        CONF_ADC_ATTENUATION,
        CONF_PIN,
        hint="Set 'attenuation:' on the adc sensor behind 'voltage:' instead",
    ),
    _validate_adc_range,
    _validate_config,
    _validate_enable_pin,
    _build_pin_source,
)


async def to_code(config: ConfigType) -> None:
    source = (
        await cg.get_variable(config[CONF_VOLTAGE])
        if CONF_VOLTAGE in config
        else await _create_internal_adc(config)
    )
    var = cg.new_Pvariable(config[CONF_ID], source)
    await cg.register_component(var, config)

    cg.add(var.set_conversion(config[CONF_CONVERSION]))
    if config[CONF_CONVERSION] == CONVERSION_DATASHEET:
        # Only a build with a datasheet model links the power law and its libm
        cg.add_define("USE_MICS_5524_DATASHEET")
    cg.add(var.set_vcc(config[CONF_VCC]))
    cg.add_optional(var.set_rl, config.get(CONF_RL))
    cg.add(var.set_voltage_multiplier(_voltage_multiplier(config)))
    cg.add(var.set_samples(config[CONF_SAMPLE_COUNT]))
    cg.add(var.set_warmup_time(config[CONF_WARMUP_TIME]))
    cg.add(var.set_preference_key(_preference_key(config)))

    await cg.add_pin(var.set_enable_pin, config.get(CONF_ENABLE_PIN))
    if (reference := config.get(CONF_AIR_REFERENCE, config.get(CONF_R0))) is not None:
        cg.add(var.set_reference(reference))
    # A request on a hub without `calibration:` uses the schema defaults, never persisted.
    calibration = config.get(CONF_CALIBRATION)
    at_boot = calibration is not None
    if calibration is None:
        calibration = CALIBRATION_SCHEMA({})
    cg.add(
        var.set_calibration(
            calibration[CONF_DELAY],
            calibration[CONF_SAMPLE_COUNT],
            calibration[CONF_SAMPLE_INTERVAL],
            at_boot and calibration[CONF_PERSIST],
            at_boot,
        )
    )

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
            resolved.p0,
            resolved.p1,
            resolved.min_ppm,
            resolved.max_ppm,
        )


async def _create_internal_adc(config: ConfigType) -> cg.MockObj:
    """Create the ``adc`` sensor of a ``pin:`` configuration and return it."""
    from esphome.components.adc import sensor as adc_sensor

    adc_config = config[_KEY_GENERATED_ADC]
    await adc_sensor.to_code(adc_config)
    return await cg.get_variable(adc_config[CONF_ID])


CALIBRATION_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.GenerateID(): cv.use_id(MiCS5524Component)}
)

automation.register_apply_action(
    "mics_5524.request_calibration",
    CALIBRATION_ACTION_SCHEMA,
    automation.ApplyCall("request_calibration()"),
)
