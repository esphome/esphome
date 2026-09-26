"""``mq_gas_sensors`` - ESPHome sensor platform for MQ gas sensors.

One entry configures one MQ sensor (MQ-2 ... MQ-309A, see ``coefficients.py``).
The analog signal comes either from an existing voltage sampler (``voltage:``,
e.g. an ``adc`` sensor) or from a pin that this component owns (``pin:``, which
creates a hidden internal ``adc`` sensor with the requested attenuation).
"""

import logging

import esphome.codegen as cg
from esphome.components import sensor, text_sensor, voltage_sampler
from esphome.components.adc.sensor import CONF_SAMPLES
from esphome.components.sensor import CONF_A, CONF_B
import esphome.config_validation as cv
from esphome.const import (
    CONF_ATTENUATION,
    CONF_CALIBRATION,
    CONF_DELAY,
    CONF_DURATION,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_INTERNAL,
    CONF_NAME,
    CONF_PIN,
    CONF_RAW,
    CONF_TEMPERATURE,
    CONF_UPDATE_INTERVAL,
    CONF_VOLTAGE,
    CONF_WARMUP_TIME,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)
from esphome.types import ConfigType

from . import (
    ADC_LIMIT_MARGIN_V,
    CONF_ADC_ATTENUATION,
    CONF_ADC_INPUT_MAX,
    CONF_ADC_PIN_MAX,
    CONF_ADC_SAMPLES,
    CONF_CORRECTION_CLAMP,
    CONF_CORRECTION_FACTOR,
    CONF_CORRECTION_MODE,
    CONF_CORRECTION_SENSOR,
    CONF_CURVE,
    CONF_GAS,
    CONF_LOG_PPM,
    CONF_LOG_SENSOR,
    CONF_MAX_PPM,
    CONF_MIN_PPM,
    CONF_PERSIST,
    CONF_R0,
    CONF_R1,
    CONF_R2,
    CONF_RATIO_IN_CLEAN_AIR,
    CONF_RATIO_MODE,
    CONF_RATIO_SENSOR,
    CONF_REGRESSION_METHOD,
    CONF_RL,
    CONF_RS_SENSOR,
    CONF_SAMPLE_INTERVAL,
    CONF_SENSOR_TYPE,
    CONF_VCC,
    CONF_VOLTAGE_MULTIPLIER,
    CONF_VOLTAGE_SENSOR,
    CORRECTION_CLAMPS,
    CORRECTION_MODES,
    ESP32_ADC_INPUT_MAX_V,
    ESP32_ADC_PIN_MAX_V,
    RATIO_MODES,
    REGRESSION_METHODS,
    MQGasSensor,
)
from .coefficients import (
    CURVE_STANDARD,
    CURVES,
    SENSOR_TYPES,
    corrected_types,
    label_for,
    normalize_gas,
    normalize_type,
    requires_coefficients,
    resolve_sensor,
    sensor_types,
)

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["sensor", "voltage_sampler", "adc"]

ADC_ATTENUATIONS = ("0db", "2.5db", "6db", "11db", "12db", "auto")


def validate_sensor_type(value: str) -> str:
    """Accept ``mq-8`` / ``MQ_8`` / ``MQ8`` and return the canonical type key."""
    key = normalize_type(value)
    if key not in SENSOR_TYPES:
        raise cv.Invalid(
            f"'{value}' is not a supported MQ sensor type, expected one of: "
            f"{', '.join(sensor_types())}"
        )
    return key


def validate_gas(value: str) -> str:
    """Normalise ``gas:`` to the keys used by the coefficient table (``h2`` -> ``H2``)."""
    return normalize_gas(value)


#: Private key under which the configuration of the generated, internal ``adc``
#: sensor is kept. It has to live inside the validated config (and not in
#: ``CORE.data``) so that ESPHome's ID pass sees the declared component ID.
_KEY_GENERATED_ADC = "_generated_adc"


def _build_internal_adc(config: ConfigType) -> ConfigType:
    """Validate the hidden ``adc`` entry of a ``pin:`` configuration.

    The ADC platform's own schema is used, so `pin:` accepts exactly what a
    ``sensor: - platform: adc`` entry accepts (pin validation, attenuation,
    multisampling, ...). It has to run during config validation - not in
    ``to_code()`` - because ESPHome's validators rely on the config path context,
    which only exists while validating.
    """
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


# The 'divider' key is written out instead of using a CONF_DIVIDER constant: emc2101
# and sprinkler already define that constant, and a third definition would have to be
# shared in esphome/components/const/__init__.py first.
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
    divider = config.get("divider")
    if divider is not None:
        return (divider[CONF_R1] + divider[CONF_R2]) / divider[CONF_R2]
    return float(config.get(CONF_VOLTAGE_MULTIPLIER, 1.0))


CALIBRATION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_RATIO_IN_CLEAN_AIR): cv.positive_not_null_float,
        cv.Optional(CONF_DELAY, default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_DURATION, default="0s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SAMPLES, default=50): cv.int_range(min=1, max=1000),
        cv.Optional(CONF_PERSIST, default=True): cv.boolean,
    }
)


def _validate_config(config: ConfigType) -> ConfigType:
    """Cross-check the coefficients and resolve the compensation mode.

    Also enforces the type/gas/coefficient combination at compile time and
    resolves ``correction_mode``: the temperature/humidity compensation is
    selected as soon as both ambient measurements are linked, ``none`` is the
    explicit opt-out and a *partial* link fails here.
    """
    type_key = config[CONF_SENSOR_TYPE]
    label = label_for(type_key)

    try:
        resolved = resolve_sensor(
            type_key,
            config.get(CONF_GAS),
            a=config.get(CONF_A),
            b=config.get(CONF_B),
            method=config.get(CONF_REGRESSION_METHOD),
            ratio_in_clean_air=config.get(CONF_RATIO_IN_CLEAN_AIR),
            rl=config.get(CONF_RL),
            vcc=config.get(CONF_VCC),
            min_ppm=config.get(CONF_MIN_PPM),
            max_ppm=config.get(CONF_MAX_PPM),
            curve=config[CONF_CURVE],
        )
    except ValueError as err:
        raise cv.Invalid(str(err)) from err

    if resolved.max_ppm <= resolved.min_ppm:
        raise cv.Invalid(
            f"max_ppm ({resolved.max_ppm}) must be greater than "
            f"min_ppm ({resolved.min_ppm})"
        )

    # Temperature/humidity compensation: `temperature:`/`humidity:` are the
    # switch, `correction_mode` only makes the choice explicit.  A partial link
    # is always a mistake (`cv.Invalid`), so `missing_sources` below is either
    # empty (both linked) or holds both keys.
    source_keys = (CONF_TEMPERATURE, CONF_HUMIDITY)
    linked = [config.get(key) is not None for key in source_keys]
    if any(linked) and not all(linked):
        missing = [key for key, ok in zip(source_keys, linked, strict=True) if not ok]
        raise cv.Invalid(
            f"'temperature:' and 'humidity:' must be linked together, "
            f"missing: {', '.join(missing)}"
        )
    missing_sources = [
        key for key, ok in zip(source_keys, linked, strict=True) if not ok
    ]

    correction_mode = config.get(CONF_CORRECTION_MODE)
    # `None` = the key was not written, i.e. the mode is derived from the links.
    explicit_mode = correction_mode
    if correction_mode is None:
        if missing_sources:
            correction_mode = "none"
            config[CONF_CORRECTION_MODE] = correction_mode
        elif resolved.tc is None:
            # The linked measurements cannot be used for this type: stay valid
            # and say so instead of failing a previously working configuration.
            correction_mode = "none"
            config[CONF_CORRECTION_MODE] = correction_mode
            _LOGGER.warning(
                "%s has no temperature/humidity correction model (supported types: "
                "%s) - the linked 'temperature:'/'humidity:' are ignored and the PPM "
                "value is published uncorrected",
                label,
                ", ".join(corrected_types()),
            )
        else:
            correction_mode = "mqdatascience"
            config[CONF_CORRECTION_MODE] = correction_mode
            _LOGGER.info(
                "%s: 'temperature:'/'humidity:' are linked - enabling the '%s' "
                "compensation (write 'correction_mode: none' to publish "
                "uncorrected values)",
                label,
                correction_mode,
            )

    if correction_mode != "none":
        if missing_sources:
            raise cv.Invalid(
                f"'correction_mode: {correction_mode}' needs ambient measurements, "
                f"missing: {', '.join(missing_sources)}"
            )
        if resolved.tc is None:
            raise cv.Invalid(
                f"{label} has no temperature/humidity correction model, "
                f"supported types: {', '.join(corrected_types())}"
            )
    elif not missing_sources and explicit_mode is not None:
        _LOGGER.info(
            "%s: temperature/humidity compensation disabled on request "
            "('correction_mode: none') - the PPM value is published uncorrected",
            label,
        )

    if resolved.curve != CURVE_STANDARD:
        _LOGGER.info(
            "%s %s: coefficient dataset '%s' selected (a=%s, b=%s, method=%s) - "
            "the published PPM values differ from the '%s' dataset",
            label,
            resolved.gas,
            resolved.curve,
            resolved.a,
            resolved.b,
            resolved.method,
            CURVE_STANDARD,
        )

    if CONF_R0 in config and CONF_CALIBRATION in config:
        _LOGGER.warning(
            "%s: both 'r0:' and 'calibration:' are set - the fixed R0 value wins and "
            "automatic calibration is disabled",
            label,
        )

    if CONF_R0 not in config and CONF_CALIBRATION not in config:
        _LOGGER.warning(
            "%s: neither 'r0:' nor 'calibration:' is set - the PPM output stays "
            "unknown until R0 is known (calibrate the sensor in clean air first)",
            label,
        )

    if resolved.heater_note:
        _LOGGER.info("%s: %s", label, resolved.heater_note)

    if requires_coefficients(type_key):
        _LOGGER.warning(
            "%s has no built-in coefficients in the reference library - make sure the "
            "'a:'/'b:' values you supplied match your sensor and target gas",
            label,
        )

    if "divider" in config and CONF_VOLTAGE_MULTIPLIER in config:
        raise cv.Invalid(
            "use either 'divider:' or 'voltage_multiplier:', not both - "
            "'divider:' already derives the multiplier from r1/r2"
        )

    # ADC range guard: the sensor output can reach VCC and the divider maps that
    # onto the ADC pin.  Only meaningful when this component owns the pin, or when
    # the limits were declared for an external sampler (e.g. an ADS1115).
    if CONF_PIN in config or CONF_ADC_INPUT_MAX in config or CONF_ADC_PIN_MAX in config:
        multiplier = _voltage_multiplier(config)
        input_max = float(config.get(CONF_ADC_INPUT_MAX, ESP32_ADC_INPUT_MAX_V))
        pin_max = float(config.get(CONF_ADC_PIN_MAX, ESP32_ADC_PIN_MAX_V))
        expected_max = float(resolved.vcc) / multiplier
        if expected_max > pin_max + ADC_LIMIT_MARGIN_V:
            raise cv.Invalid(
                f"the sensor output can reach {resolved.vcc:.2f} V and the configured "
                f"divider maps that to {expected_max:.2f} V on the ADC pin, above "
                f"'adc_pin_max' ({pin_max:.2f} V) - that can damage the pin. Use "
                f"'divider: {{r1: 10.0, r2: 20.0}}' (-> 1.5) or "
                f"'{{r1: 10.0, r2: 10.0}}' (-> 2.0), or set 'adc_pin_max' to your "
                f"sampler's limit (6.144 for an ADS1115)"
            )
        if expected_max > input_max + ADC_LIMIT_MARGIN_V:
            _LOGGER.warning(
                "%s: the sensor output can reach %.2f V and the configured divider maps "
                "that to %.2f V at the ADC input, above 'adc_input_max' (%.2f V): the top "
                "of the range clips or reads non-linearly. 10k/20k gives 3.33 V - raise "
                "'adc_input_max' if that is intentional",
                label,
                resolved.vcc,
                expected_max,
                input_max,
            )

    if CONF_PIN in config:
        config[_KEY_GENERATED_ADC] = _build_internal_adc(config)

    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        MQGasSensor,
        unit_of_measurement=UNIT_PARTS_PER_MILLION,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:molecule",
    )
    .extend(
        {
            cv.Required(CONF_SENSOR_TYPE): cv.All(cv.string, validate_sensor_type),
            cv.Optional(CONF_GAS): cv.All(cv.string, validate_gas),
            cv.Optional(CONF_VOLTAGE): cv.use_id(voltage_sampler.VoltageSampler),
            cv.Optional(CONF_PIN): cv.valid,
            cv.SplitDefault(CONF_ADC_ATTENUATION, esp32="12db"): cv.All(
                cv.only_on_esp32, cv.one_of(*ADC_ATTENUATIONS, lower=True)
            ),
            cv.Optional(CONF_ADC_SAMPLES, default=1): cv.int_range(min=1, max=255),
            cv.Optional(CONF_VOLTAGE_MULTIPLIER): cv.positive_not_null_float,
            cv.Optional("divider"): DIVIDER_SCHEMA,
            cv.Optional(CONF_ADC_INPUT_MAX): cv.positive_not_null_float,
            cv.Optional(CONF_ADC_PIN_MAX): cv.positive_not_null_float,
            cv.Optional(CONF_VCC, default=5.0): cv.positive_not_null_float,
            cv.Optional(CONF_RL, default=10.0): cv.positive_not_null_float,
            cv.Optional(CONF_R0): cv.positive_not_null_float,
            cv.Optional(CONF_A): cv.float_,
            cv.Optional(CONF_B): cv.float_,
            cv.Optional(CONF_REGRESSION_METHOD): cv.one_of(
                *REGRESSION_METHODS, lower=True
            ),
            cv.Optional(CONF_RATIO_MODE, default="rs_r0"): cv.one_of(
                *RATIO_MODES, lower=True
            ),
            cv.Optional(CONF_RATIO_IN_CLEAN_AIR): cv.positive_not_null_float,
            cv.Optional(CONF_SAMPLES, default=2): cv.int_range(min=1, max=255),
            cv.Optional(
                CONF_SAMPLE_INTERVAL, default="20ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_WARMUP_TIME, default="0s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MIN_PPM): cv.float_,
            cv.Optional(CONF_MAX_PPM): cv.positive_not_null_float,
            cv.Optional(CONF_CORRECTION_FACTOR, default=0.0): cv.float_,
            cv.Optional(CONF_CORRECTION_MODE): cv.one_of(*CORRECTION_MODES, lower=True),
            cv.Optional(CONF_CORRECTION_CLAMP, default="absolute"): cv.one_of(
                *CORRECTION_CLAMPS, lower=True
            ),
            cv.Optional(CONF_CURVE, default=CURVE_STANDARD): cv.one_of(
                *CURVES, lower=True
            ),
            cv.Optional(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_HUMIDITY): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_CALIBRATION): CALIBRATION_SCHEMA,
            cv.Optional(CONF_RATIO_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_RS_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_VOLTAGE_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_CORRECTION_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_LOG_SENSOR): cv.use_id(text_sensor.TextSensor),
            cv.Optional(CONF_LOG_PPM, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("60s")),
    cv.has_exactly_one_key(CONF_VOLTAGE, CONF_PIN),
    _validate_config,
)


async def _create_internal_adc(config: ConfigType):
    """Instantiate the hidden, internal ``adc`` sensor used by ``pin:`` configurations.

    The entry was validated by :func:`_build_internal_adc`, here the ADC platform's
    own ``to_code`` is reused so attenuation, sampling count, calibration and
    platform specifics behave exactly like a hand written
    ``sensor: - platform: adc`` entry.
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
    resolved = resolve_sensor(
        config[CONF_SENSOR_TYPE],
        config.get(CONF_GAS),
        a=config.get(CONF_A),
        b=config.get(CONF_B),
        method=config.get(CONF_REGRESSION_METHOD),
        ratio_in_clean_air=config.get(CONF_RATIO_IN_CLEAN_AIR),
        rl=config.get(CONF_RL),
        vcc=config.get(CONF_VCC),
        min_ppm=config.get(CONF_MIN_PPM),
        max_ppm=config.get(CONF_MAX_PPM),
        curve=config[CONF_CURVE],
    )

    calibration = config.get(CONF_CALIBRATION)
    ratio_in_clean_air = resolved.ratio_in_clean_air
    if calibration is not None and CONF_RATIO_IN_CLEAN_AIR in calibration:
        ratio_in_clean_air = calibration[CONF_RATIO_IN_CLEAN_AIR]

    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    cg.add(var.set_type(resolved.label))
    cg.add(var.set_gas(resolved.gas))
    cg.add(var.set_a(resolved.a))
    cg.add(var.set_b(resolved.b))
    cg.add(var.set_regression_method(REGRESSION_METHODS[resolved.method]))
    cg.add(var.set_ratio_mode(RATIO_MODES[config[CONF_RATIO_MODE]]))
    cg.add(var.set_rl(resolved.rl))
    cg.add(var.set_vcc(resolved.vcc))
    cg.add(var.set_ratio_in_clean_air(ratio_in_clean_air))
    cg.add(var.set_min_ppm(resolved.min_ppm))
    cg.add(var.set_max_ppm(resolved.max_ppm))
    cg.add(var.set_voltage_multiplier(_voltage_multiplier(config)))
    cg.add(var.set_samples(config[CONF_SAMPLES]))
    cg.add(var.set_sample_interval(config[CONF_SAMPLE_INTERVAL]))
    cg.add(var.set_warmup_time(config[CONF_WARMUP_TIME]))
    cg.add(var.set_correction_factor(config[CONF_CORRECTION_FACTOR]))
    cg.add(var.set_correction_mode(CORRECTION_MODES[config[CONF_CORRECTION_MODE]]))
    cg.add(var.set_correction_clamp(CORRECTION_CLAMPS[config[CONF_CORRECTION_CLAMP]]))

    if config[CONF_CORRECTION_MODE] != "none":
        tc = resolved.tc
        if tc is None:  # pragma: no cover - already rejected by _validate_config
            raise ValueError(
                f"correction_mode is enabled but {resolved.label} has no "
                "temperature/humidity correction model"
            )
        cg.add(var.set_tc_coefficients(tc.a33, tc.b33, tc.c33, tc.a85, tc.b85, tc.c85))
        cg.add(
            var.set_temperature_source(await cg.get_variable(config[CONF_TEMPERATURE]))
        )
        cg.add(var.set_humidity_source(await cg.get_variable(config[CONF_HUMIDITY])))

    if (r0 := config.get(CONF_R0)) is not None:
        cg.add(var.set_r0(r0))

    if calibration is not None and CONF_R0 not in config:
        cg.add(
            var.set_calibration(
                True,
                ratio_in_clean_air,
                calibration[CONF_DELAY],
                calibration[CONF_DURATION],
                calibration[CONF_SAMPLES],
                calibration[CONF_PERSIST],
            )
        )

    if source_id := config.get(CONF_VOLTAGE):
        cg.add(var.set_source(await cg.get_variable(source_id)))
    else:
        cg.add(var.set_source(await _create_internal_adc(config)))

    cg.add(var.set_log_ppm(config[CONF_LOG_PPM]))

    for key, setter in (
        (CONF_RATIO_SENSOR, var.set_ratio_sensor),
        (CONF_RS_SENSOR, var.set_rs_sensor),
        (CONF_VOLTAGE_SENSOR, var.set_voltage_sensor),
        (CONF_CORRECTION_SENSOR, var.set_correction_sensor),
        (CONF_LOG_SENSOR, var.set_log_sensor),
    ):
        if (target := config.get(key)) is not None:
            cg.add(setter(await cg.get_variable(target)))

    _LOGGER.debug(
        "%s %s: a=%s b=%s method=%s curve=%s ratio_in_clean_air=%s rl=%s vcc=%s "
        "range=%s..%s ppm correction=%s",
        resolved.label,
        resolved.gas,
        resolved.a,
        resolved.b,
        resolved.method,
        resolved.curve,
        ratio_in_clean_air,
        resolved.rl,
        resolved.vcc,
        resolved.min_ppm,
        resolved.max_ppm,
        config[CONF_CORRECTION_MODE],
    )
