"""``mq_gas_sensors`` - ESPHome sensor platform for MQ gas sensors.

One entry configures one MQ sensor (MQ-2 ... MQ-309A, see ``coefficients.py``).
The analog signal comes either from an existing voltage sampler (``voltage:``,
e.g. an ``adc`` sensor) or from a pin that this component owns (``pin:``, which
creates a hidden internal ``adc`` sensor with the requested attenuation).
"""

import logging
import math

from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import sensor, text_sensor, voltage_sampler
from esphome.components.adc import ATTENUATION_MODES, DOMAIN as ADC_DOMAIN
from esphome.components.esp32 import VARIANT_ESP32S31, get_esp32_variant
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
    CONF_PLATFORM,
    CONF_RAW,
    CONF_TEMPERATURE,
    CONF_UPDATE_INTERVAL,
    CONF_VOLTAGE,
    CONF_VOLTAGE_DIVIDER,
    CONF_WARMUP_TIME,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
    UNIT_VOLT,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.helpers import fnv1_hash
from esphome.types import ConfigType

from . import (
    ADC_LIMIT_MARGIN_V,
    CONF_ADC_ATTENUATION,
    CONF_ADC_INPUT_MAX,
    CONF_ADC_PIN_MAX,
    CONF_ADC_SAMPLES,
    CONF_COEFFICIENT_A,
    CONF_COEFFICIENT_B,
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
    CONF_R0_SENSOR,
    CONF_R1,
    CONF_R2,
    CONF_RATIO_IN_CLEAN_AIR,
    CONF_RATIO_MODE,
    CONF_RATIO_SENSOR,
    CONF_REGRESSION_METHOD,
    CONF_RL,
    CONF_RS_SENSOR,
    CONF_SAMPLE_COUNT,
    CONF_SAMPLE_INTERVAL,
    CONF_SENSOR_TYPE,
    CONF_VCC,
    CONF_VOLTAGE_MULTIPLIER,
    CONF_VOLTAGE_SENSOR,
    CORRECTION_CLAMPS,
    CORRECTION_MODES,
    ESP32_ADC_INPUT_MAX_V,
    ESP32_ADC_PIN_MAX_V,
    ESP8266_ADC_INPUT_MAX_V,
    RATIO_MODES,
    REGRESSION_METHODS,
    MQGasSensor,
    esp32_attenuation_input_max,
    esp32_attenuation_range_is_known,
)
from .coefficients import (
    CURVE_STANDARD,
    CURVES,
    SENSOR_TYPES,
    ResolvedSensor,
    corrected_types,
    label_for,
    normalize_gas,
    normalize_type,
    requires_coefficients,
    resolve_sensor,
    sensor_types,
)

_LOGGER = logging.getLogger(__name__)


def _auto_load(config: ConfigType | None) -> list[str]:
    """Load every platform the configuration actually uses.

    ``adc`` is only needed for a ``pin:`` source, where the internal sampler is
    built, and ``text_sensor`` only for the ``log_sensor:`` mirror, whose C++ code
    is compiled in behind ``USE_MQ_GAS_SENSORS_LOG_SENSOR``.
    """
    loaded = ["sensor", "voltage_sampler"]
    if config is not None:
        if CONF_PIN in config:
            loaded.append("adc")
        if config.get(CONF_LOG_SENSOR) is not None:
            loaded.append("text_sensor")
    return loaded


AUTO_LOAD = _auto_load

#: Attenuation of the internal ADC of a ``pin:`` configuration when
#: ``adc_attenuation:`` is not written: the widest range, which is also what the
#: divider guard measures against.
DEFAULT_ADC_ATTENUATION = "12db"

#: The ESP32-S31 ADC supports a single attenuation level, so the widest range of
#: the other ESP32 variants is not available there.
_ESP32_S31_ATTENUATION = "0db"

MIN_SAMPLE_INTERVAL_MS = 16

MAX_SAMPLE_COUNT = 16

#: At most this many analog-to-digital conversions per reading once the internal
#: ADC's own averaging is multiplied in: every ``sample()`` of an owned ``adc``
#: runs its whole ``adc_samples:`` burst inside the same loop pass as the reading.
MAX_CONVERSIONS_PER_READING = 64


def validate_sensor_type(value: str) -> str:
    key = normalize_type(value)
    if key not in SENSOR_TYPES:
        raise cv.Invalid(
            f"'{value}' is not a supported MQ sensor type, expected one of: "
            f"{', '.join(sensor_types())}"
        )
    return key


def validate_gas(value: str) -> str:
    return normalize_gas(value)


def validate_finite(value: float) -> float:
    """Reject a non-finite number, which the firmware cannot evaluate.

    ``clamp_a()`` / ``clamp_b()`` turn a non-finite coefficient into 0, which
    drops the curve without a word, and a non-finite correction factor makes
    every ratio unknown.
    """
    if not math.isfinite(value):
        raise cv.Invalid(f"must be a finite number, got {value}")
    return value


#: Floats the firmware can evaluate. ``positive_not_null_float`` already rejects
#: NaN but accepts infinity, which turns every derived value into zero or infinity.
positive_finite_float = cv.All(cv.positive_not_null_float, validate_finite)
finite_float = cv.All(cv.float_, validate_finite)


#: Private key under which the configuration of the generated, internal ``adc``
#: sensor is kept. It has to live inside the validated config (and not in
#: ``CORE.data``) so that ESPHome's ID pass sees the declared component ID.
_KEY_GENERATED_ADC = "_generated_adc"


def _default_adc_attenuation() -> str:
    """Attenuation of the owned ADC when ``adc_attenuation:`` is not written."""
    if CORE.is_esp32 and get_esp32_variant() == VARIANT_ESP32S31:
        return _ESP32_S31_ATTENUATION
    return DEFAULT_ADC_ATTENUATION


def _build_internal_adc(config: ConfigType) -> ConfigType:
    """Validate the hidden ``adc`` entry of a ``pin:`` configuration.

    The ADC platform's own schema is used, so `pin:` accepts exactly what a
    ``sensor: - platform: adc`` entry accepts (pin validation, attenuation,
    multisampling, ...). It has to run during config validation - not in
    ``to_code()`` - because ESPHome's validators rely on the config path context,
    which only exists while validating.
    """
    from esphome.components.adc import sensor as adc_sensor

    # A suffix a YAML id is unlikely to use: the generated id must not collide with
    # one the user wrote, which would read as an "ID ... redefined" error.
    adc_id = f"{config[CONF_ID].id}_internal_adc"
    adc_config: ConfigType = {
        CONF_ID: adc_id,
        CONF_PIN: config[CONF_PIN],
        CONF_NAME: f"{adc_id} voltage",
        CONF_INTERNAL: True,
        CONF_RAW: False,
        adc_sensor.CONF_SAMPLING_MODE: "avg",
        CONF_UPDATE_INTERVAL: "never",
    }
    if (samples := config.get(CONF_ADC_SAMPLES)) is not None:
        adc_config[adc_sensor.CONF_SAMPLES] = samples
    if (attenuation := config.get(CONF_ADC_ATTENUATION)) is not None:
        adc_config[CONF_ATTENUATION] = attenuation
    elif CORE.is_esp32:
        # The owned ADC of an ESP32 keeps the widest range unless the YAML asks
        # for another one - the same default the divider guard assumes.
        adc_config[CONF_ATTENUATION] = _default_adc_attenuation()

    return adc_sensor.CONFIG_SCHEMA(adc_config)


DIVIDER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_R1): positive_finite_float,
        cv.Required(CONF_R2): positive_finite_float,
    }
)


def _voltage_multiplier(config: ConfigType) -> float:
    """Inverse of the divider ratio, derived from the wiring if possible.

    ``voltage_divider: {r1: 10.0, r2: 20.0}`` (kOhm, r1 in series with the sensor
    output, r2 to ground) documents the hardware and yields ``(r1 + r2) / r2`` =
    1.5 for the recommended 10k/20k.  ``voltage_multiplier:`` stays available as
    the low-level escape hatch; it also covers a directly connected sensor (1.0) or
    a sampler with a wider input range, e.g. an ADS1115.
    """
    divider = config.get(CONF_VOLTAGE_DIVIDER)
    if divider is not None:
        return (divider[CONF_R1] + divider[CONF_R2]) / divider[CONF_R2]
    return float(config.get(CONF_VOLTAGE_MULTIPLIER, 1.0))


def _preference_key(
    vcc: float, multiplier: float, rl: float, ratio_in_clean_air: float
) -> int:
    """Identity of the wiring a stored R0 was measured with.

    A calibration derives ``R0 = RS / ratio_in_clean_air`` from a clean-air
    reading, so a value kept in flash only describes the sensor while the supply,
    the scaling, the load resistor and the clean-air ratio stay the same. All four
    go into the key, so a configuration change never reads back an R0 that belongs
    to different hardware.
    """
    return fnv1_hash(f"{vcc:g}:{multiplier:g}:{rl:g}:{ratio_in_clean_air:g}")


CALIBRATION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_RATIO_IN_CLEAN_AIR): positive_finite_float,
        cv.Optional(CONF_DELAY, default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_DURATION, default="0s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SAMPLE_COUNT, default=50): cv.int_range(min=1, max=1000),
        cv.Optional(CONF_PERSIST, default=True): cv.boolean,
    }
)


def _resolve_sensor(config: ConfigType) -> ResolvedSensor:
    """Resolve the curve of the entry.

    The one place that lists the configuration keys the coefficients are taken
    from, used by the validator and by the code generation alike, so the two
    cannot drift apart.
    """
    return resolve_sensor(
        config[CONF_SENSOR_TYPE],
        config.get(CONF_GAS),
        a=config.get(CONF_COEFFICIENT_A),
        b=config.get(CONF_COEFFICIENT_B),
        method=config.get(CONF_REGRESSION_METHOD),
        ratio_in_clean_air=config.get(CONF_RATIO_IN_CLEAN_AIR),
        rl=config.get(CONF_RL),
        vcc=config.get(CONF_VCC),
        min_ppm=config.get(CONF_MIN_PPM),
        max_ppm=config.get(CONF_MAX_PPM),
        curve=config[CONF_CURVE],
    )


def _validate_coefficients(label: str, method: str, a: float, b: float) -> None:
    """Reject a coefficient pair the regression cannot evaluate (log10 of a <= 0, 1/b)."""
    if method == "exponential":
        usable = a > 0
        need = "'coefficient_a:' > 0"
    elif method == "inverse":
        usable = a > 0 and b != 0
        need = "'coefficient_a:' > 0 and a non-zero 'coefficient_b:'"
    else:
        usable = a != 0
        need = "a non-zero 'coefficient_a:'"
    if not usable:
        raise cv.Invalid(
            f"{label} with 'regression_method: {method}' needs {need}, got "
            f"coefficient_a={a:g}, coefficient_b={b:g} - set 'coefficient_a:' and "
            f"'coefficient_b:' for this method or drop 'regression_method:'"
        )


def _adc_input_max(attenuation: str) -> tuple[float, bool]:
    """Measuring range of the internal ADC at an attenuation, for this target.

    The second value says whether the range comes from a table row for the target.
    It does not for an ESP32 variant the attenuation table has no entry for, where
    the narrowest known range is only a safe guess and a declared
    ``adc_input_max:`` is authoritative.

    The attenuation only describes the range on ESP32: A0 of the ESP8266 is a
    fixed 0 - 1.0 V input and the other targets use the recommended maximum input
    voltage.
    """
    if CORE.is_esp32:
        variant = get_esp32_variant()
        return (
            esp32_attenuation_input_max(variant, attenuation),
            esp32_attenuation_range_is_known(variant),
        )
    if CORE.is_esp8266:
        return ESP8266_ADC_INPUT_MAX_V, True
    return ESP32_ADC_INPUT_MAX_V, True


def _check_adc_range(
    config: ConfigType, vcc: float, input_max: float, pin_max: float
) -> None:
    """Refuse a wiring that puts more on the ADC pin than it survives or measures.

    The sensor output can reach ``vcc`` and the divider maps that onto the ADC
    pin, so ``vcc`` divided by the multiplier is the largest voltage the pin ever
    sees.
    """
    expected_max = vcc / _voltage_multiplier(config)
    if expected_max > pin_max + ADC_LIMIT_MARGIN_V:
        raise cv.Invalid(
            f"the sensor output can reach {vcc:.2f} V and the configured "
            f"divider maps that to {expected_max:.2f} V on the ADC pin, above "
            f"'adc_pin_max' ({pin_max:.2f} V) - that can damage the pin. Use "
            f"'voltage_divider: {{r1: 10.0, r2: 20.0}}' (-> 1.5) or "
            f"'{{r1: 10.0, r2: 10.0}}' (-> 2.0), or set 'adc_pin_max' to your "
            f"sampler's limit (6.144 for an ADS1115)"
        )
    if expected_max > input_max + ADC_LIMIT_MARGIN_V:
        raise cv.Invalid(
            f"the sensor output can reach {vcc:.2f} V and the configured "
            f"wiring maps that to {expected_max:.2f} V at the ADC input, above the "
            f"{input_max:.2f} V measuring range of the sampler - raise 'r1'/'r2', "
            "or declare 'adc_input_max:' for a sampler with a wider range"
        )


def _effective_input_max(
    label: str, config: ConfigType, known_max: float, known: bool
) -> float:
    """Range the divider is checked against.

    A declared ``adc_input_max:`` describes a sampler whose range the
    configuration otherwise cannot, but it cannot widen an ADC past what its
    attenuation measures: above that voltage the conversion flattens, so the
    reading silently stops tracking the sensor.
    """
    declared = config.get(CONF_ADC_INPUT_MAX)
    if declared is None:
        return known_max
    if not known or declared <= known_max:
        return float(declared)
    _LOGGER.warning(
        "%s: 'adc_input_max: %.2f' is above the %.2f V the ADC measures at its "
        "attenuation - the divider is checked against %.2f V, and conversions "
        "above it flatten",
        label,
        declared,
        known_max,
        known_max,
    )
    return known_max


def _validate_config(config: ConfigType) -> ConfigType:
    """Cross-check the coefficients and resolve the compensation mode.

    Also enforces the type/gas/coefficient combination at compile time and
    resolves ``correction_mode``: the temperature/humidity compensation is
    selected as soon as both ambient measurements are linked, ``none`` is the
    explicit opt-out and a *partial* link fails here.
    """
    if CONF_VOLTAGE in config:
        for key in (CONF_ADC_ATTENUATION, CONF_ADC_SAMPLES):
            if key in config:
                raise cv.Invalid(
                    f"'{key}:' configures the internal ADC of a 'pin:' sensor and has "
                    "no effect with 'voltage:' - configure the sampler itself instead"
                )

    # The pin is validated with the ADC platform's own schema, and first: a bad
    # pin has to be reported as such, not as one of the checks below.
    if CONF_PIN in config:
        config[_KEY_GENERATED_ADC] = _build_internal_adc(config)

        # Every `sample()` of the owned ADC averages `adc_samples:` conversions, and
        # `sample_count` of those run back to back inside one pass of the main loop.
        adc_samples = config.get(CONF_ADC_SAMPLES, 1)
        conversions = config[CONF_SAMPLE_COUNT] * adc_samples
        if conversions > MAX_CONVERSIONS_PER_READING:
            raise cv.Invalid(
                f"'sample_count: {config[CONF_SAMPLE_COUNT]}' with 'adc_samples: "
                f"{adc_samples}' samples the analog output {conversions} times per "
                f"reading, and the conversions run back to back inside one pass of "
                f"the main loop - at most {MAX_CONVERSIONS_PER_READING} are allowed, "
                "lower 'sample_count:' or 'adc_samples:'"
            )

    type_key = config[CONF_SENSOR_TYPE]
    label = label_for(type_key)

    try:
        resolved = _resolve_sensor(config)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err

    _validate_coefficients(resolved.label, resolved.method, resolved.a, resolved.b)

    # The device clamps `a` / `b` to what it can evaluate (see `MQ_MAX_A`), and
    # every clamp is reported: a user supplied coefficient here, the one built-in
    # fit above the limit below.
    for key, given, evaluated in (
        (CONF_COEFFICIENT_A, config.get(CONF_COEFFICIENT_A), resolved.a),
        (CONF_COEFFICIENT_B, config.get(CONF_COEFFICIENT_B), resolved.b),
    ):
        if given is not None and given != evaluated:
            _LOGGER.warning(
                "%s: '%s: %g' is outside the range the firmware evaluates and is "
                "clamped to %g - the curve it uses differs from the one you wrote",
                label,
                key,
                given,
                evaluated,
            )

    if resolved.built_in_clamped:
        _LOGGER.warning(
            "%s %s: a built-in coefficient is outside the range the firmware evaluates and "
            "is clamped to a=%g, b=%g - the curve is then not the published one and "
            "reports the top of the range for almost every ratio, set "
            "'coefficient_a:' and 'coefficient_b:' for a usable curve",
            resolved.label,
            resolved.gas,
            resolved.a,
            resolved.b,
        )

    # The correction factor moves the measured ratio, so the value clean air is
    # expected to read is the ratio plus the factor. `r0_from_clean_air()` sets
    # R0 = RS_air / ratio_in_clean_air in either mode, so `r0_rs` evaluates the
    # inverse of the ratio above.
    ratio_in_clean_air = resolved.ratio_in_clean_air
    calibration = config.get(CONF_CALIBRATION)
    if calibration is not None and CONF_RATIO_IN_CLEAN_AIR in calibration:
        ratio_in_clean_air = calibration[CONF_RATIO_IN_CLEAN_AIR]
    clean_air_ratio = ratio_in_clean_air
    if config[CONF_RATIO_MODE] == "r0_rs":
        clean_air_ratio = 1.0 / clean_air_ratio
    correction_factor = float(config[CONF_CORRECTION_FACTOR])
    if clean_air_ratio + correction_factor <= 0.0:
        raise cv.Invalid(
            f"'correction_factor: {correction_factor:g}' moves the clean air ratio of "
            f"{label} ({clean_air_ratio:g} against 'ratio_mode: {config[CONF_RATIO_MODE]}') "
            f"to {clean_air_ratio + correction_factor:g} - clean air would no longer be "
            "inside the range the curve describes"
        )

    if resolved.max_ppm <= resolved.min_ppm:
        raise cv.Invalid(
            f"max_ppm ({resolved.max_ppm}) must be greater than "
            f"min_ppm ({resolved.min_ppm})"
        )

    # `temperature:`/`humidity:` select the compensation together; linking only
    # one of them is always a mistake.
    source_keys = (CONF_TEMPERATURE, CONF_HUMIDITY)
    linked = [config.get(key) is not None for key in source_keys]
    missing_sources = [
        key for key, ok in zip(source_keys, linked, strict=True) if not ok
    ]
    if missing_sources and len(missing_sources) < len(source_keys):
        raise cv.Invalid(
            f"'temperature:' and 'humidity:' must be linked together, "
            f"missing: {', '.join(missing_sources)}"
        )

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
                "%s has no temperature/humidity correction model - the linked "
                "'temperature:'/'humidity:' are ignored",
                label,
            )
        else:
            correction_mode = "mqdatascience"
            config[CONF_CORRECTION_MODE] = correction_mode
            _LOGGER.info(
                "%s: 'temperature:'/'humidity:' are linked - enabling the '%s' "
                "compensation (write 'correction_mode: none' to opt out)",
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
            "%s: temperature/humidity compensation disabled on request",
            label,
        )

    if resolved.curve != CURVE_STANDARD:
        _LOGGER.info(
            "%s %s: coefficient dataset '%s' selected (a=%s, b=%s, method=%s)",
            label,
            resolved.gas,
            resolved.curve,
            resolved.a,
            resolved.b,
            resolved.method,
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
            "'coefficient_a:'/'coefficient_b:' values you supplied match your sensor "
            "and target gas",
            label,
        )

    # ADC range guard: the sensor output can reach VCC and the divider maps that
    # onto the ADC pin.  Only meaningful when this component owns the pin, or when
    # the limits were declared for an external sampler (e.g. an ADS1115); the
    # sampler `voltage:` reads from is checked in `_final_validate_voltage_source()`
    # below, where its configuration can be looked up.
    if CONF_PIN in config or CONF_ADC_INPUT_MAX in config or CONF_ADC_PIN_MAX in config:
        if CONF_PIN in config:
            # The configured attenuation sets the measuring range of the internal ADC.
            input_max, known = _adc_input_max(
                config.get(CONF_ADC_ATTENUATION, _default_adc_attenuation())
            )
        else:
            # Without `pin:` the sampler is only known at final validation; here the
            # declared value is all there is to check the divider against.
            input_max, known = ESP32_ADC_INPUT_MAX_V, False
        _check_adc_range(
            config,
            resolved.vcc,
            _effective_input_max(label, config, input_max, known),
            float(config.get(CONF_ADC_PIN_MAX, ESP32_ADC_PIN_MAX_V)),
        )

    return config


def _final_validate_voltage_source(config: ConfigType) -> ConfigType:
    """Check the sampler `voltage:` reads from and the wiring that feeds it.

    ``voltage:`` names another component's sensor, so what it reads from is only
    known once the whole configuration is validated - that is what final validation
    is for.  An internal ``adc`` sensor carries the attenuation its measuring range
    follows, so the same range and burst checks as for an owned ``pin:`` run against
    it, and one set to output raw counts is refused outright; any other sampler has a
    range of its own that has to be declared with ``adc_input_max:``.
    """
    if CONF_VOLTAGE not in config:
        return config

    from esphome.components.adc import sensor as adc_sensor

    sampler = fv.get_declaring_config(config[CONF_VOLTAGE])
    if sampler.get(CONF_PLATFORM) != ADC_DOMAIN:
        # A declared range is what `_validate_config` checked the divider against;
        # without one, nothing here says what an external sampler measures.
        if CONF_ADC_INPUT_MAX not in config:
            _LOGGER.warning(
                "%s: 'voltage:' reads from another sampler, whose input range cannot "
                "be deduced from the configuration - declare 'adc_input_max:' (and "
                "'adc_pin_max:' when it takes more) so the wiring is checked against it",
                label_for(config[CONF_SENSOR_TYPE]),
            )
        return config

    if sampler.get(CONF_RAW):
        # The `sample()` of such a sensor returns ADC counts, not volts, so every
        # reading would be a number of counts taken for a voltage.
        raise cv.Invalid(
            "'voltage:' reads from an internal ADC sensor that outputs raw counts "
            "('raw: true') - remove 'raw:' from it, it cannot be used as a voltage "
            "source"
        )

    # The sampler runs its own `samples:` conversions inside every `sample()`, and
    # `sample_count` of those run back to back in one pass of the main loop.
    samples = sampler.get(adc_sensor.CONF_SAMPLES, 1)
    conversions = config[CONF_SAMPLE_COUNT] * samples
    if conversions > MAX_CONVERSIONS_PER_READING:
        raise cv.Invalid(
            f"'sample_count: {config[CONF_SAMPLE_COUNT]}' with the 'samples: "
            f"{samples}' of the ADC sensor 'voltage:' reads from samples the analog "
            f"output {conversions} times per reading, and those conversions run back "
            f"to back inside one pass of the main loop - at most "
            f"{MAX_CONVERSIONS_PER_READING} are allowed, lower 'sample_count:' or "
            "the sampler's 'samples:'"
        )

    input_max, known = _adc_input_max(
        sampler.get(CONF_ATTENUATION, adc_sensor.DEFAULT_ATTENUATION)
    )
    _check_adc_range(
        config,
        float(config[CONF_VCC]),
        _effective_input_max(
            label_for(config[CONF_SENSOR_TYPE]), config, input_max, known
        ),
        float(config.get(CONF_ADC_PIN_MAX, ESP32_ADC_PIN_MAX_V)),
    )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate_voltage_source


def _validate_esp32_attenuation(value: str) -> str:
    """Reject an attenuation the ADC of this ESP32 variant does not have."""
    if (
        CORE.is_esp32
        and get_esp32_variant() == VARIANT_ESP32S31
        and value != _ESP32_S31_ATTENUATION
    ):
        raise cv.Invalid(
            f"'adc_attenuation: {value}' is not available on ESP32-S31, whose "
            "ADC has a single attenuation - remove 'adc_attenuation:' or set "
            f"'adc_attenuation: {_ESP32_S31_ATTENUATION}'"
        )
    return value


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
            cv.Optional(CONF_ADC_ATTENUATION): cv.All(
                cv.only_on_esp32,
                cv.one_of(*ATTENUATION_MODES, lower=True),
                _validate_esp32_attenuation,
            ),
            cv.Optional(CONF_ADC_SAMPLES): cv.int_range(min=1, max=255),
            cv.Optional(CONF_VOLTAGE_MULTIPLIER): positive_finite_float,
            cv.Optional(CONF_VOLTAGE_DIVIDER): DIVIDER_SCHEMA,
            cv.Optional(CONF_ADC_INPUT_MAX): positive_finite_float,
            cv.Optional(CONF_ADC_PIN_MAX): positive_finite_float,
            cv.Optional(CONF_VCC, default=5.0): positive_finite_float,
            cv.Optional(CONF_RL, default=10.0): positive_finite_float,
            cv.Optional(CONF_R0): positive_finite_float,
            cv.Optional(CONF_COEFFICIENT_A): finite_float,
            cv.Optional(CONF_COEFFICIENT_B): finite_float,
            cv.Optional(CONF_REGRESSION_METHOD): cv.one_of(
                *REGRESSION_METHODS, lower=True
            ),
            cv.Optional(CONF_RATIO_MODE, default="rs_r0"): cv.one_of(
                *RATIO_MODES, lower=True
            ),
            cv.Optional(CONF_RATIO_IN_CLEAN_AIR): positive_finite_float,
            cv.Optional(CONF_SAMPLE_COUNT, default=2): cv.int_range(
                min=1, max=MAX_SAMPLE_COUNT
            ),
            cv.Optional(CONF_SAMPLE_INTERVAL, default="20ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.Range(min=cv.TimePeriod(milliseconds=MIN_SAMPLE_INTERVAL_MS)),
            ),
            cv.Optional(
                CONF_WARMUP_TIME, default="0s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MIN_PPM): finite_float,
            cv.Optional(CONF_MAX_PPM): positive_finite_float,
            cv.Optional(CONF_CORRECTION_FACTOR, default=0.0): finite_float,
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
            # The mirrors of the measurement chain are owned by this component and
            # carry the diagnostic category, so Home Assistant keeps them out of
            # the way of the PPM entity.
            cv.Optional(CONF_RATIO_SENSOR): sensor.sensor_schema(
                accuracy_decimals=3,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_RS_SENSOR): sensor.sensor_schema(
                unit_of_measurement="kOhm",
                accuracy_decimals=3,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_VOLTAGE_SENSOR): sensor.sensor_schema(
                unit_of_measurement=UNIT_VOLT,
                accuracy_decimals=4,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_CORRECTION_SENSOR): sensor.sensor_schema(
                accuracy_decimals=4,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_R0_SENSOR): sensor.sensor_schema(
                unit_of_measurement="kOhm",
                accuracy_decimals=4,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_LOG_SENSOR): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_LOG_PPM, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("60s")),
    cv.has_exactly_one_key(CONF_VOLTAGE, CONF_PIN),
    cv.has_at_most_one_key(CONF_VOLTAGE_DIVIDER, CONF_VOLTAGE_MULTIPLIER),
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
    resolved = _resolve_sensor(config)

    calibration = config.get(CONF_CALIBRATION)
    ratio_in_clean_air = resolved.ratio_in_clean_air
    if calibration is not None and CONF_RATIO_IN_CLEAN_AIR in calibration:
        ratio_in_clean_air = calibration[CONF_RATIO_IN_CLEAN_AIR]

    if (source_id := config.get(CONF_VOLTAGE)) is not None:
        source = await cg.get_variable(source_id)
    else:
        source = await _create_internal_adc(config)
    var = await sensor.new_sensor(config, source)
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
    multiplier = _voltage_multiplier(config)
    cg.add(var.set_voltage_multiplier(multiplier))
    cg.add(
        var.set_preference_key(
            _preference_key(resolved.vcc, multiplier, resolved.rl, ratio_in_clean_air)
        )
    )
    cg.add(var.set_samples(config[CONF_SAMPLE_COUNT]))
    cg.add(var.set_sample_interval(config[CONF_SAMPLE_INTERVAL]))
    cg.add(var.set_warmup_time(config[CONF_WARMUP_TIME]))
    cg.add(var.set_correction_factor(config[CONF_CORRECTION_FACTOR]))
    cg.add(var.set_correction_mode(CORRECTION_MODES[config[CONF_CORRECTION_MODE]]))
    cg.add(var.set_correction_clamp(CORRECTION_CLAMPS[config[CONF_CORRECTION_CLAMP]]))

    if config[CONF_CORRECTION_MODE] != "none":
        # Only a build that can use the model compiles it in: without the define
        # `compute_correction_()` returns 1.0 and the ratio is used as it is read.
        cg.add_define("USE_MQ_GAS_SENSORS_CORRECTION")
        tc = resolved.tc
        if tc is None:  # pragma: no cover - already rejected by _validate_config
            raise ValueError(
                f"correction_mode is enabled but {resolved.label} has no "
                "temperature/humidity correction model"
            )
        cg.add(var.set_tc_coefficients(tc.a33, tc.b33, tc.c33, tc.a85, tc.b85, tc.c85))
        await cg.add_variable(var.set_temperature_source, config[CONF_TEMPERATURE])
        await cg.add_variable(var.set_humidity_source, config[CONF_HUMIDITY])

    cg.add_optional(var.set_r0, config.get(CONF_R0))

    if calibration is not None and CONF_R0 not in config:
        cg.add(
            var.set_calibration(
                True,
                ratio_in_clean_air,
                calibration[CONF_DELAY],
                calibration[CONF_DURATION],
                calibration[CONF_SAMPLE_COUNT],
                calibration[CONF_PERSIST],
            )
        )

    cg.add(var.set_log_ppm(config[CONF_LOG_PPM]))

    # The mirror is the only user of `text_sensor` here, so the member, its header
    # and the publish call are only compiled in when it is configured.
    if CONF_LOG_SENSOR in config:
        cg.add_define("USE_MQ_GAS_SENSORS_LOG_SENSOR")

    # The diagnostics are this component's own entities: they are created from its
    # configuration instead of being wired to sensors the user has to declare.
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_RATIO_SENSOR, var.set_ratio_sensor)
    await sensors(CONF_RS_SENSOR, var.set_rs_sensor)
    await sensors(CONF_VOLTAGE_SENSOR, var.set_voltage_sensor)
    await sensors(CONF_CORRECTION_SENSOR, var.set_correction_sensor)
    await sensors(CONF_R0_SENSOR, var.set_r0_sensor)
    logs = text_sensor.sub_text_sensors(config)
    await logs(CONF_LOG_SENSOR, var.set_log_sensor)

    _LOGGER.debug(
        "%s %s: coefficient_a=%s coefficient_b=%s method=%s curve=%s "
        "ratio_in_clean_air=%s rl=%s vcc=%s range=%s..%s ppm correction=%s",
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


CALIBRATE_R0_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(MQGasSensor),
    }
)


automation.register_apply_action(
    "mq_gas_sensors.calibrate_r0",
    CALIBRATE_R0_ACTION_SCHEMA,
    automation.ApplyCall("start_calibration()"),
)
