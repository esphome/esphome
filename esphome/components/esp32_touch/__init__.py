import esphome.codegen as cg
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
from esphome.components import esp32
from esphome.components.esp32 import get_esp32_variant, gpio
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)

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

TOUCH_PADS = {
    VARIANT_ESP32: {
        4: cg.global_ns.TOUCH_PAD_NUM0,
        0: cg.global_ns.TOUCH_PAD_NUM1,
        2: cg.global_ns.TOUCH_PAD_NUM2,
        15: cg.global_ns.TOUCH_PAD_NUM3,
        13: cg.global_ns.TOUCH_PAD_NUM4,
        12: cg.global_ns.TOUCH_PAD_NUM5,
        14: cg.global_ns.TOUCH_PAD_NUM6,
        27: cg.global_ns.TOUCH_PAD_NUM7,
        33: cg.global_ns.TOUCH_PAD_NUM8,
        32: cg.global_ns.TOUCH_PAD_NUM9,
    },
    VARIANT_ESP32S2: {
        1: cg.global_ns.TOUCH_PAD_NUM1,
        2: cg.global_ns.TOUCH_PAD_NUM2,
        3: cg.global_ns.TOUCH_PAD_NUM3,
        4: cg.global_ns.TOUCH_PAD_NUM4,
        5: cg.global_ns.TOUCH_PAD_NUM5,
        6: cg.global_ns.TOUCH_PAD_NUM6,
        7: cg.global_ns.TOUCH_PAD_NUM7,
        8: cg.global_ns.TOUCH_PAD_NUM8,
        9: cg.global_ns.TOUCH_PAD_NUM9,
        10: cg.global_ns.TOUCH_PAD_NUM10,
        11: cg.global_ns.TOUCH_PAD_NUM11,
        12: cg.global_ns.TOUCH_PAD_NUM12,
        13: cg.global_ns.TOUCH_PAD_NUM13,
        14: cg.global_ns.TOUCH_PAD_NUM14,
    },
    VARIANT_ESP32S3: {
        1: cg.global_ns.TOUCH_PAD_NUM1,
        2: cg.global_ns.TOUCH_PAD_NUM2,
        3: cg.global_ns.TOUCH_PAD_NUM3,
        4: cg.global_ns.TOUCH_PAD_NUM4,
        5: cg.global_ns.TOUCH_PAD_NUM5,
        6: cg.global_ns.TOUCH_PAD_NUM6,
        7: cg.global_ns.TOUCH_PAD_NUM7,
        8: cg.global_ns.TOUCH_PAD_NUM8,
        9: cg.global_ns.TOUCH_PAD_NUM9,
        10: cg.global_ns.TOUCH_PAD_NUM10,
        11: cg.global_ns.TOUCH_PAD_NUM11,
        12: cg.global_ns.TOUCH_PAD_NUM12,
        13: cg.global_ns.TOUCH_PAD_NUM13,
        14: cg.global_ns.TOUCH_PAD_NUM14,
    },
}


TOUCH_PAD_DENOISE_GRADE = {
    "BIT12": cg.global_ns.TOUCH_PAD_DENOISE_BIT12,
    "BIT10": cg.global_ns.TOUCH_PAD_DENOISE_BIT10,
    "BIT8": cg.global_ns.TOUCH_PAD_DENOISE_BIT8,
    "BIT4": cg.global_ns.TOUCH_PAD_DENOISE_BIT4,
}

TOUCH_PAD_DENOISE_CAP_LEVEL = {
    "L0": cg.global_ns.TOUCH_PAD_DENOISE_CAP_L0,
    "L1": cg.global_ns.TOUCH_PAD_DENOISE_CAP_L1,
    "L2": cg.global_ns.TOUCH_PAD_DENOISE_CAP_L2,
    "L3": cg.global_ns.TOUCH_PAD_DENOISE_CAP_L3,
    "L4": cg.global_ns.TOUCH_PAD_DENOISE_CAP_L4,
    "L5": cg.global_ns.TOUCH_PAD_DENOISE_CAP_L5,
    "L6": cg.global_ns.TOUCH_PAD_DENOISE_CAP_L6,
    "L7": cg.global_ns.TOUCH_PAD_DENOISE_CAP_L7,
}

TOUCH_PAD_FILTER_MODE = {
    "IIR_4": cg.global_ns.TOUCH_PAD_FILTER_IIR_4,
    "IIR_8": cg.global_ns.TOUCH_PAD_FILTER_IIR_8,
    "IIR_16": cg.global_ns.TOUCH_PAD_FILTER_IIR_16,
    "IIR_32": cg.global_ns.TOUCH_PAD_FILTER_IIR_32,
    "IIR_64": cg.global_ns.TOUCH_PAD_FILTER_IIR_64,
    "IIR_128": cg.global_ns.TOUCH_PAD_FILTER_IIR_128,
    "IIR_256": cg.global_ns.TOUCH_PAD_FILTER_IIR_256,
    "JITTER": cg.global_ns.TOUCH_PAD_FILTER_JITTER,
}

TOUCH_PAD_SMOOTH_MODE = {
    "OFF": cg.global_ns.TOUCH_PAD_SMOOTH_OFF,
    "IIR_2": cg.global_ns.TOUCH_PAD_SMOOTH_IIR_2,
    "IIR_4": cg.global_ns.TOUCH_PAD_SMOOTH_IIR_4,
    "IIR_8": cg.global_ns.TOUCH_PAD_SMOOTH_IIR_8,
}

LOW_VOLTAGE_REFERENCE = {
    "0.5V": cg.global_ns.TOUCH_LVOLT_0V5,
    "0.6V": cg.global_ns.TOUCH_LVOLT_0V6,
    "0.7V": cg.global_ns.TOUCH_LVOLT_0V7,
    "0.8V": cg.global_ns.TOUCH_LVOLT_0V8,
}
HIGH_VOLTAGE_REFERENCE = {
    "2.4V": cg.global_ns.TOUCH_HVOLT_2V4,
    "2.5V": cg.global_ns.TOUCH_HVOLT_2V5,
    "2.6V": cg.global_ns.TOUCH_HVOLT_2V6,
    "2.7V": cg.global_ns.TOUCH_HVOLT_2V7,
}
VOLTAGE_ATTENUATION = {
    "1.5V": cg.global_ns.TOUCH_HVOLT_ATTEN_1V5,
    "1V": cg.global_ns.TOUCH_HVOLT_ATTEN_1V,
    "0.5V": cg.global_ns.TOUCH_HVOLT_ATTEN_0V5,
    "0V": cg.global_ns.TOUCH_HVOLT_ATTEN_0V,
}
TOUCH_PAD_WATERPROOF_SHIELD_DRIVER = {
    "L0": cg.global_ns.TOUCH_PAD_SHIELD_DRV_L0,
    "L1": cg.global_ns.TOUCH_PAD_SHIELD_DRV_L1,
    "L2": cg.global_ns.TOUCH_PAD_SHIELD_DRV_L2,
    "L3": cg.global_ns.TOUCH_PAD_SHIELD_DRV_L3,
    "L4": cg.global_ns.TOUCH_PAD_SHIELD_DRV_L4,
    "L5": cg.global_ns.TOUCH_PAD_SHIELD_DRV_L5,
    "L6": cg.global_ns.TOUCH_PAD_SHIELD_DRV_L6,
    "L7": cg.global_ns.TOUCH_PAD_SHIELD_DRV_L7,
}


def validate_touch_pad(value):
    value = gpio.gpio_pin_number_validator(value)
    variant = get_esp32_variant()
    if variant not in TOUCH_PADS:
        raise cv.Invalid(f"ESP32 variant {variant} does not support touch pads.")

    pads = TOUCH_PADS[variant]
    if value not in pads:
        raise cv.Invalid(f"Pin {value} does not support touch pads.")
    return cv.enum(pads)(value)


def validate_variant_vars(config):
    if get_esp32_variant() == VARIANT_ESP32:
        variant_vars = {
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
        for vvar in variant_vars:
            if vvar in config:
                raise cv.Invalid(f"{vvar} is not valid on {VARIANT_ESP32}")
    elif (
        get_esp32_variant() == VARIANT_ESP32S2 or get_esp32_variant() == VARIANT_ESP32S3
    ) and CONF_IIR_FILTER in config:
        raise cv.Invalid(
            f"{CONF_IIR_FILTER} is not valid on {VARIANT_ESP32S2} or {VARIANT_ESP32S3}"
        )

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
            cv.Optional(CONF_VOLTAGE_ATTENUATION, default="0V"): validate_voltage(
                VOLTAGE_ATTENUATION
            ),
            # ESP32 only
            cv.Optional(CONF_IIR_FILTER): cv.positive_time_period_milliseconds,
            # ESP32-S2/S3 only
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
            cv.Optional(CONF_WATERPROOF_SHIELD_DRIVER): cv.enum(
                TOUCH_PAD_WATERPROOF_SHIELD_DRIVER, upper=True, space="_"
            ),
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
            esp32.const.VARIANT_ESP32,
            esp32.const.VARIANT_ESP32S2,
            esp32.const.VARIANT_ESP32S3,
        ]
    ),
    validate_variant_vars,
)


async def to_code(config):
    touch = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(touch, config)

    cg.add(touch.set_setup_mode(config[CONF_SETUP_MODE]))

    sleep_duration = int(round(config[CONF_SLEEP_DURATION].total_microseconds * 0.15))
    cg.add(touch.set_sleep_duration(sleep_duration))

    measurement_duration = int(
        round(config[CONF_MEASUREMENT_DURATION].total_microseconds * 7.99987793)
    )
    cg.add(touch.set_measurement_duration(measurement_duration))

    cg.add(
        touch.set_low_voltage_reference(
            LOW_VOLTAGE_REFERENCE[config[CONF_LOW_VOLTAGE_REFERENCE]]
        )
    )
    cg.add(
        touch.set_high_voltage_reference(
            HIGH_VOLTAGE_REFERENCE[config[CONF_HIGH_VOLTAGE_REFERENCE]]
        )
    )
    cg.add(
        touch.set_voltage_attenuation(
            VOLTAGE_ATTENUATION[config[CONF_VOLTAGE_ATTENUATION]]
        )
    )

    if get_esp32_variant() == VARIANT_ESP32:
        if CONF_IIR_FILTER in config:
            cg.add(touch.set_iir_filter(config[CONF_IIR_FILTER]))

    if get_esp32_variant() == VARIANT_ESP32S2 or get_esp32_variant() == VARIANT_ESP32S3:
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
