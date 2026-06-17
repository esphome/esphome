import logging

import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.esp32 import (
    VARIANT_ESP32,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    get_esp32_variant,
    gpio,
    include_builtin_idf_component,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_HIGH_VOLTAGE_REFERENCE,
    CONF_ID,
    CONF_IIR_FILTER,
    CONF_LOW_VOLTAGE_REFERENCE,
    CONF_MEASUREMENT_DURATION,
    CONF_SETUP_MODE,
    CONF_SLEEP_DURATION,
    CONF_VOLTAGE_ATTENUATION,
)
from esphome.core import TimePeriod

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["binary_sensor"]
DEPENDENCIES = ["esp32"]

CONF_DEBOUNCE_COUNT = "debounce_count"
CONF_DENOISE_GRADE = "denoise_grade"
CONF_DENOISE_CAP_LEVEL = "denoise_cap_level"
CONF_FILTER_MODE = "filter_mode"
CONF_NOISE_THRESHOLD = "noise_threshold"
CONF_JITTER_STEP = "jitter_step"
CONF_SMOOTH_MODE = "smooth_mode"
CONF_WATERPROOF_GUARD_RING = "waterproof_guard_ring"
CONF_WATERPROOF_SHIELD_DRIVER = "waterproof_shield_driver"

esp32_touch_ns = cg.esphome_ns.namespace("esp32_touch")
ESP32TouchComponent = esp32_touch_ns.class_("ESP32TouchComponent", cg.Component)

# Channel ID mappings: GPIO pin number -> integer channel ID
# These are plain integers - the new unified API uses int chan_id directly.
TOUCH_PADS = {
    VARIANT_ESP32: {
        4: 0,
        0: 1,
        2: 2,
        15: 3,
        13: 4,
        12: 5,
        14: 6,
        27: 7,
        33: 8,
        32: 9,
    },
    VARIANT_ESP32S2: {
        1: 1,
        2: 2,
        3: 3,
        4: 4,
        5: 5,
        6: 6,
        7: 7,
        8: 8,
        9: 9,
        10: 10,
        11: 11,
        12: 12,
        13: 13,
        14: 14,
    },
    VARIANT_ESP32S3: {
        1: 1,
        2: 2,
        3: 3,
        4: 4,
        5: 5,
        6: 6,
        7: 7,
        8: 8,
        9: 9,
        10: 10,
        11: 11,
        12: 12,
        13: 13,
        14: 14,
    },
    VARIANT_ESP32P4: {
        2: 1,
        3: 2,
        4: 3,
        5: 4,
        6: 5,
        7: 6,
        8: 7,
        9: 8,
        10: 9,
        11: 10,
        12: 11,
        13: 12,
        14: 13,
        15: 14,
    },
}


TOUCH_PAD_DENOISE_GRADE = {
    "BIT12": cg.global_ns.TOUCH_DENOISE_CHAN_RESOLUTION_BIT12,
    "BIT10": cg.global_ns.TOUCH_DENOISE_CHAN_RESOLUTION_BIT10,
    "BIT8": cg.global_ns.TOUCH_DENOISE_CHAN_RESOLUTION_BIT8,
    "BIT4": cg.global_ns.TOUCH_DENOISE_CHAN_RESOLUTION_BIT4,
}

TOUCH_PAD_DENOISE_CAP_LEVEL = {
    "L0": cg.global_ns.TOUCH_DENOISE_CHAN_CAP_5PF,
    "L1": cg.global_ns.TOUCH_DENOISE_CHAN_CAP_6PF,
    "L2": cg.global_ns.TOUCH_DENOISE_CHAN_CAP_7PF,
    "L3": cg.global_ns.TOUCH_DENOISE_CHAN_CAP_9PF,
    "L4": cg.global_ns.TOUCH_DENOISE_CHAN_CAP_10PF,
    "L5": cg.global_ns.TOUCH_DENOISE_CHAN_CAP_12PF,
    "L6": cg.global_ns.TOUCH_DENOISE_CHAN_CAP_13PF,
    "L7": cg.global_ns.TOUCH_DENOISE_CHAN_CAP_14PF,
}

TOUCH_PAD_FILTER_MODE = {
    "IIR_4": cg.global_ns.TOUCH_BM_IIR_FILTER_4,
    "IIR_8": cg.global_ns.TOUCH_BM_IIR_FILTER_8,
    "IIR_16": cg.global_ns.TOUCH_BM_IIR_FILTER_16,
    "IIR_32": cg.global_ns.TOUCH_BM_IIR_FILTER_32,
    "IIR_64": cg.global_ns.TOUCH_BM_IIR_FILTER_64,
    "IIR_128": cg.global_ns.TOUCH_BM_IIR_FILTER_128,
    "IIR_256": cg.global_ns.TOUCH_BM_IIR_FILTER_256,
    "JITTER": cg.global_ns.TOUCH_BM_JITTER_FILTER,
}

TOUCH_PAD_SMOOTH_MODE = {
    "OFF": cg.global_ns.TOUCH_SMOOTH_NO_FILTER,
    "IIR_2": cg.global_ns.TOUCH_SMOOTH_IIR_FILTER_2,
    "IIR_4": cg.global_ns.TOUCH_SMOOTH_IIR_FILTER_4,
    "IIR_8": cg.global_ns.TOUCH_SMOOTH_IIR_FILTER_8,
}

LOW_VOLTAGE_REFERENCE = {
    "0.5V": cg.global_ns.TOUCH_VOLT_LIM_L_0V5,
    "0.6V": cg.global_ns.TOUCH_VOLT_LIM_L_0V6,
    "0.7V": cg.global_ns.TOUCH_VOLT_LIM_L_0V7,
    "0.8V": cg.global_ns.TOUCH_VOLT_LIM_L_0V8,
}
HIGH_VOLTAGE_REFERENCE = {
    "2.4V": cg.global_ns.TOUCH_VOLT_LIM_H_2V4,
    "2.5V": cg.global_ns.TOUCH_VOLT_LIM_H_2V5,
    "2.6V": cg.global_ns.TOUCH_VOLT_LIM_H_2V6,
    "2.7V": cg.global_ns.TOUCH_VOLT_LIM_H_2V7,
}
VOLTAGE_ATTENUATION = {"1.5V", "1V", "0.5V", "0V"}

# ESP32 V1: The new API's touch_volt_lim_h_t combines the old high_voltage_reference
# and voltage_attenuation into a single enum representing the effective upper voltage.
# Effective voltage = high_voltage_reference - voltage_attenuation
EFFECTIVE_HIGH_VOLTAGE = {
    ("2.4V", "1.5V"): cg.global_ns.TOUCH_VOLT_LIM_H_0V9,
    ("2.5V", "1.5V"): cg.global_ns.TOUCH_VOLT_LIM_H_1V0,
    ("2.6V", "1.5V"): cg.global_ns.TOUCH_VOLT_LIM_H_1V1,
    ("2.7V", "1.5V"): cg.global_ns.TOUCH_VOLT_LIM_H_1V2,
    ("2.4V", "1V"): cg.global_ns.TOUCH_VOLT_LIM_H_1V4,
    ("2.5V", "1V"): cg.global_ns.TOUCH_VOLT_LIM_H_1V5,
    ("2.6V", "1V"): cg.global_ns.TOUCH_VOLT_LIM_H_1V6,
    ("2.7V", "1V"): cg.global_ns.TOUCH_VOLT_LIM_H_1V7,
    ("2.4V", "0.5V"): cg.global_ns.TOUCH_VOLT_LIM_H_1V9,
    ("2.5V", "0.5V"): cg.global_ns.TOUCH_VOLT_LIM_H_2V0,
    ("2.6V", "0.5V"): cg.global_ns.TOUCH_VOLT_LIM_H_2V1,
    ("2.7V", "0.5V"): cg.global_ns.TOUCH_VOLT_LIM_H_2V2,
    ("2.4V", "0V"): cg.global_ns.TOUCH_VOLT_LIM_H_2V4,
    ("2.5V", "0V"): cg.global_ns.TOUCH_VOLT_LIM_H_2V5,
    ("2.6V", "0V"): cg.global_ns.TOUCH_VOLT_LIM_H_2V6,
    ("2.7V", "0V"): cg.global_ns.TOUCH_VOLT_LIM_H_2V7,
}


def validate_touch_pad(value):
    value = gpio.gpio_pin_number_validator(value)
    variant = get_esp32_variant()
    pads = TOUCH_PADS.get(variant)
    if pads is None:
        raise cv.Invalid(f"ESP32 variant {variant} does not support touch pads.")
    if value not in pads:
        raise cv.Invalid(f"Pin {value} does not support touch pads.")
    return pads[value]  # Return integer channel ID


def validate_variant_vars(config):
    variant = get_esp32_variant()
    invalid_vars = set()
    if variant == VARIANT_ESP32:
        invalid_vars = {
            CONF_DEBOUNCE_COUNT,
            CONF_DENOISE_GRADE,
            CONF_DENOISE_CAP_LEVEL,
            CONF_FILTER_MODE,
            CONF_NOISE_THRESHOLD,
            CONF_JITTER_STEP,
            CONF_SMOOTH_MODE,
            CONF_WATERPROOF_GUARD_RING,
            CONF_WATERPROOF_SHIELD_DRIVER,
        }
    elif variant in (VARIANT_ESP32S2, VARIANT_ESP32S3, VARIANT_ESP32P4):
        invalid_vars = {CONF_IIR_FILTER}
        if variant == VARIANT_ESP32P4:
            invalid_vars |= {CONF_DENOISE_GRADE, CONF_DENOISE_CAP_LEVEL}
    unsupported = invalid_vars.intersection(config)
    if unsupported:
        keys = ", ".join(sorted(f"'{k}'" for k in unsupported))
        raise cv.Invalid(f"{keys} not valid on {variant}")

    return config


def validate_voltage(values):
    def validator(value):
        if isinstance(value, float) and value.is_integer():
            value = int(value)
        value = cv.string(value)
        if not value.endswith("V"):
            value += "V"
        return cv.one_of(*values)(value)

    return validator


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESP32TouchComponent),
            cv.Optional(CONF_SETUP_MODE, default=False): cv.boolean,
            # common options
            cv.Optional(CONF_SLEEP_DURATION, default="27306us"): cv.All(
                cv.positive_time_period, cv.Range(max=TimePeriod(microseconds=436906))
            ),
            cv.Optional(CONF_MEASUREMENT_DURATION, default="8192us"): cv.All(
                cv.positive_time_period, cv.Range(max=TimePeriod(microseconds=8192))
            ),
            cv.Optional(CONF_LOW_VOLTAGE_REFERENCE, default="0.5V"): validate_voltage(
                LOW_VOLTAGE_REFERENCE
            ),
            cv.Optional(CONF_HIGH_VOLTAGE_REFERENCE, default="2.7V"): validate_voltage(
                HIGH_VOLTAGE_REFERENCE
            ),
            # ESP32 V1 only: attenuates the high voltage reference
            cv.SplitDefault(
                CONF_VOLTAGE_ATTENUATION,
                esp32="0V",
                esp32_s2=cv.UNDEFINED,
                esp32_s3=cv.UNDEFINED,
                esp32_p4=cv.UNDEFINED,
            ): validate_voltage(VOLTAGE_ATTENUATION),
            # ESP32 only
            cv.Optional(CONF_IIR_FILTER): cv.positive_time_period_milliseconds,
            # ESP32-S2/S3/P4 only
            cv.Optional(CONF_DEBOUNCE_COUNT): cv.int_range(min=0, max=7),
            cv.Optional(CONF_FILTER_MODE): cv.enum(
                TOUCH_PAD_FILTER_MODE, upper=True, space="_"
            ),
            cv.Optional(CONF_NOISE_THRESHOLD): cv.int_range(min=0, max=3),
            cv.Optional(CONF_JITTER_STEP): cv.int_range(min=0, max=15),
            cv.Optional(CONF_SMOOTH_MODE): cv.enum(
                TOUCH_PAD_SMOOTH_MODE, upper=True, space="_"
            ),
            cv.Optional(CONF_DENOISE_GRADE): cv.enum(
                TOUCH_PAD_DENOISE_GRADE, upper=True, space="_"
            ),
            cv.Optional(CONF_DENOISE_CAP_LEVEL): cv.enum(
                TOUCH_PAD_DENOISE_CAP_LEVEL, upper=True, space="_"
            ),
            cv.Optional(CONF_WATERPROOF_GUARD_RING): validate_touch_pad,
            cv.Optional(CONF_WATERPROOF_SHIELD_DRIVER): cv.int_range(min=0, max=7),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_none_or_all_keys(CONF_DENOISE_GRADE, CONF_DENOISE_CAP_LEVEL),
    cv.has_none_or_all_keys(
        CONF_DEBOUNCE_COUNT,
        CONF_FILTER_MODE,
        CONF_NOISE_THRESHOLD,
        CONF_JITTER_STEP,
        CONF_SMOOTH_MODE,
    ),
    cv.has_none_or_all_keys(CONF_WATERPROOF_GUARD_RING, CONF_WATERPROOF_SHIELD_DRIVER),
    esp32.only_on_variant(
        supported=[
            esp32.VARIANT_ESP32,
            esp32.VARIANT_ESP32S2,
            esp32.VARIANT_ESP32S3,
            esp32.VARIANT_ESP32P4,
        ]
    ),
    validate_variant_vars,
)


async def to_code(config):
    # New unified touch sensor driver
    include_builtin_idf_component("esp_driver_touch_sens")

    touch = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(touch, config)

    cg.add(touch.set_setup_mode(config[CONF_SETUP_MODE]))

    # sleep_duration -> meas_interval_us (pass microseconds directly)
    cg.add(touch.set_meas_interval_us(config[CONF_SLEEP_DURATION].total_microseconds))

    variant = get_esp32_variant()

    # measurement_duration handling differs per variant
    if variant == VARIANT_ESP32:
        # V1: charge_duration_ms (convert from microseconds to milliseconds)
        charge_duration_ms = (
            config[CONF_MEASUREMENT_DURATION].total_microseconds / 1000.0
        )
        cg.add(touch.set_charge_duration_ms(charge_duration_ms))
    else:
        # V2/V3: charge_times (approximate conversion from duration)
        # The old API used clock cycles; the new API uses charge_times count.
        # Default is 500 for V2/V3. Use measurement_duration as a rough scaling factor.
        # 65535 / 8192 ≈ 7.9999 maps the microsecond duration to charge_times.
        charge_times = int(
            round(config[CONF_MEASUREMENT_DURATION].total_microseconds * (65535 / 8192))
        )
        charge_times = max(charge_times, 1)
        cg.add(touch.set_charge_times(charge_times))

    # Voltage references (not applicable to P4)
    if variant != VARIANT_ESP32P4:
        if CONF_LOW_VOLTAGE_REFERENCE in config:
            cg.add(
                touch.set_low_voltage_reference(
                    LOW_VOLTAGE_REFERENCE[config[CONF_LOW_VOLTAGE_REFERENCE]]
                )
            )
        if CONF_HIGH_VOLTAGE_REFERENCE in config:
            if variant == VARIANT_ESP32:
                # V1: combine high_voltage_reference with voltage_attenuation
                high_ref = config[CONF_HIGH_VOLTAGE_REFERENCE]
                atten = config[CONF_VOLTAGE_ATTENUATION]
                cg.add(
                    touch.set_high_voltage_reference(
                        EFFECTIVE_HIGH_VOLTAGE[(high_ref, atten)]
                    )
                )
            else:
                # V2/V3: no attenuation concept, use directly
                cg.add(
                    touch.set_high_voltage_reference(
                        HIGH_VOLTAGE_REFERENCE[config[CONF_HIGH_VOLTAGE_REFERENCE]]
                    )
                )

    if variant == VARIANT_ESP32 and CONF_IIR_FILTER in config:
        cg.add(touch.set_iir_filter(config[CONF_IIR_FILTER]))

    if variant in (VARIANT_ESP32S2, VARIANT_ESP32S3, VARIANT_ESP32P4):
        if CONF_FILTER_MODE in config:
            cg.add(touch.set_filter_mode(config[CONF_FILTER_MODE]))
        if CONF_DEBOUNCE_COUNT in config:
            cg.add(touch.set_debounce_count(config[CONF_DEBOUNCE_COUNT]))
        if CONF_NOISE_THRESHOLD in config:
            cg.add(touch.set_noise_threshold(config[CONF_NOISE_THRESHOLD]))
        if CONF_JITTER_STEP in config:
            cg.add(touch.set_jitter_step(config[CONF_JITTER_STEP]))
        if CONF_SMOOTH_MODE in config:
            cg.add(touch.set_smooth_level(config[CONF_SMOOTH_MODE]))
        if CONF_DENOISE_GRADE in config:
            cg.add(touch.set_denoise_grade(config[CONF_DENOISE_GRADE]))
        if CONF_DENOISE_CAP_LEVEL in config:
            cg.add(touch.set_denoise_cap(config[CONF_DENOISE_CAP_LEVEL]))
        if CONF_WATERPROOF_GUARD_RING in config:
            cg.add(
                touch.set_waterproof_guard_ring_pad(config[CONF_WATERPROOF_GUARD_RING])
            )
        if CONF_WATERPROOF_SHIELD_DRIVER in config:
            cg.add(
                touch.set_waterproof_shield_driver(
                    config[CONF_WATERPROOF_SHIELD_DRIVER]
                )
            )
