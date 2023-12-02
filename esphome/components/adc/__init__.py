import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ANALOG, CONF_INPUT

from esphome.core import CORE
from esphome.components.esp32 import get_esp32_variant
from esphome.const import PLATFORM_ESP8266
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)

CODEOWNERS = ["@esphome/core"]

ATTENUATION_MODES = {
    "0db": cg.global_ns.ADC_ATTEN_DB_0,
    "2.5db": cg.global_ns.ADC_ATTEN_DB_2_5,
    "6db": cg.global_ns.ADC_ATTEN_DB_6,
    "11db": cg.global_ns.ADC_ATTEN_DB_11,
    "auto": "auto",
}

adc1_channel_t = cg.global_ns.enum("adc1_channel_t")
adc2_channel_t = cg.global_ns.enum("adc2_channel_t")

# From https://github.com/espressif/esp-idf/blob/master/components/driver/include/driver/adc_common.h
# pin to adc1 channel mapping
ESP32_VARIANT_ADC1_PIN_TO_CHANNEL = {
    VARIANT_ESP32: {
        36: adc1_channel_t.ADC1_CHANNEL_0,
        37: adc1_channel_t.ADC1_CHANNEL_1,
        38: adc1_channel_t.ADC1_CHANNEL_2,
        39: adc1_channel_t.ADC1_CHANNEL_3,
        32: adc1_channel_t.ADC1_CHANNEL_4,
        33: adc1_channel_t.ADC1_CHANNEL_5,
        34: adc1_channel_t.ADC1_CHANNEL_6,
        35: adc1_channel_t.ADC1_CHANNEL_7,
    },
    VARIANT_ESP32S2: {
        1: adc1_channel_t.ADC1_CHANNEL_0,
        2: adc1_channel_t.ADC1_CHANNEL_1,
        3: adc1_channel_t.ADC1_CHANNEL_2,
        4: adc1_channel_t.ADC1_CHANNEL_3,
        5: adc1_channel_t.ADC1_CHANNEL_4,
        6: adc1_channel_t.ADC1_CHANNEL_5,
        7: adc1_channel_t.ADC1_CHANNEL_6,
        8: adc1_channel_t.ADC1_CHANNEL_7,
        9: adc1_channel_t.ADC1_CHANNEL_8,
        10: adc1_channel_t.ADC1_CHANNEL_9,
    },
    VARIANT_ESP32S3: {
        1: adc1_channel_t.ADC1_CHANNEL_0,
        2: adc1_channel_t.ADC1_CHANNEL_1,
        3: adc1_channel_t.ADC1_CHANNEL_2,
        4: adc1_channel_t.ADC1_CHANNEL_3,
        5: adc1_channel_t.ADC1_CHANNEL_4,
        6: adc1_channel_t.ADC1_CHANNEL_5,
        7: adc1_channel_t.ADC1_CHANNEL_6,
        8: adc1_channel_t.ADC1_CHANNEL_7,
        9: adc1_channel_t.ADC1_CHANNEL_8,
        10: adc1_channel_t.ADC1_CHANNEL_9,
    },
    VARIANT_ESP32C3: {
        0: adc1_channel_t.ADC1_CHANNEL_0,
        1: adc1_channel_t.ADC1_CHANNEL_1,
        2: adc1_channel_t.ADC1_CHANNEL_2,
        3: adc1_channel_t.ADC1_CHANNEL_3,
        4: adc1_channel_t.ADC1_CHANNEL_4,
    },
    VARIANT_ESP32C2: {
        0: adc1_channel_t.ADC1_CHANNEL_0,
        1: adc1_channel_t.ADC1_CHANNEL_1,
        2: adc1_channel_t.ADC1_CHANNEL_2,
        3: adc1_channel_t.ADC1_CHANNEL_3,
        4: adc1_channel_t.ADC1_CHANNEL_4,
    },
    VARIANT_ESP32C6: {
        0: adc1_channel_t.ADC1_CHANNEL_0,
        1: adc1_channel_t.ADC1_CHANNEL_1,
        2: adc1_channel_t.ADC1_CHANNEL_2,
        3: adc1_channel_t.ADC1_CHANNEL_3,
        4: adc1_channel_t.ADC1_CHANNEL_4,
        5: adc1_channel_t.ADC1_CHANNEL_5,
        6: adc1_channel_t.ADC1_CHANNEL_6,
    },
    VARIANT_ESP32H2: {
        0: adc1_channel_t.ADC1_CHANNEL_0,
        1: adc1_channel_t.ADC1_CHANNEL_1,
        2: adc1_channel_t.ADC1_CHANNEL_2,
        3: adc1_channel_t.ADC1_CHANNEL_3,
        4: adc1_channel_t.ADC1_CHANNEL_4,
    },
}

ESP32_VARIANT_ADC2_PIN_TO_CHANNEL = {
    # TODO: add other variants
    VARIANT_ESP32: {
        4: adc2_channel_t.ADC2_CHANNEL_0,
        0: adc2_channel_t.ADC2_CHANNEL_1,
        2: adc2_channel_t.ADC2_CHANNEL_2,
        15: adc2_channel_t.ADC2_CHANNEL_3,
        13: adc2_channel_t.ADC2_CHANNEL_4,
        12: adc2_channel_t.ADC2_CHANNEL_5,
        14: adc2_channel_t.ADC2_CHANNEL_6,
        27: adc2_channel_t.ADC2_CHANNEL_7,
        25: adc2_channel_t.ADC2_CHANNEL_8,
        26: adc2_channel_t.ADC2_CHANNEL_9,
    },
    VARIANT_ESP32S2: {
        11: adc2_channel_t.ADC2_CHANNEL_0,
        12: adc2_channel_t.ADC2_CHANNEL_1,
        13: adc2_channel_t.ADC2_CHANNEL_2,
        14: adc2_channel_t.ADC2_CHANNEL_3,
        15: adc2_channel_t.ADC2_CHANNEL_4,
        16: adc2_channel_t.ADC2_CHANNEL_5,
        17: adc2_channel_t.ADC2_CHANNEL_6,
        18: adc2_channel_t.ADC2_CHANNEL_7,
        19: adc2_channel_t.ADC2_CHANNEL_8,
        20: adc2_channel_t.ADC2_CHANNEL_9,
    },
    VARIANT_ESP32S3: {
        11: adc2_channel_t.ADC2_CHANNEL_0,
        12: adc2_channel_t.ADC2_CHANNEL_1,
        13: adc2_channel_t.ADC2_CHANNEL_2,
        14: adc2_channel_t.ADC2_CHANNEL_3,
        15: adc2_channel_t.ADC2_CHANNEL_4,
        16: adc2_channel_t.ADC2_CHANNEL_5,
        17: adc2_channel_t.ADC2_CHANNEL_6,
        18: adc2_channel_t.ADC2_CHANNEL_7,
        19: adc2_channel_t.ADC2_CHANNEL_8,
        20: adc2_channel_t.ADC2_CHANNEL_9,
    },
    VARIANT_ESP32C3: {
        5: adc2_channel_t.ADC2_CHANNEL_0,
    },
}


def validate_adc_pin(value):
    if str(value).upper() == "VCC":
        if CORE.is_rp2040:
            return pins.internal_gpio_input_pin_schema(29)
        return cv.only_on([PLATFORM_ESP8266])("VCC")

    if str(value).upper() == "TEMPERATURE":
        return cv.only_on_rp2040("TEMPERATURE")

    if CORE.is_esp32:
        value = pins.internal_gpio_input_pin_number(value)
        variant = get_esp32_variant()
        if (
            variant not in ESP32_VARIANT_ADC1_PIN_TO_CHANNEL
            and variant not in ESP32_VARIANT_ADC2_PIN_TO_CHANNEL
        ):
            raise cv.Invalid(f"This ESP32 variant ({variant}) is not supported")

        if (
            value not in ESP32_VARIANT_ADC1_PIN_TO_CHANNEL[variant]
            and value not in ESP32_VARIANT_ADC2_PIN_TO_CHANNEL[variant]
        ):
            raise cv.Invalid(f"{variant} doesn't support ADC on this pin")

        return pins.internal_gpio_input_pin_schema(value)

    if CORE.is_esp8266:
        value = pins.internal_gpio_pin_number({CONF_ANALOG: True, CONF_INPUT: True})(
            value
        )

        if value != 17:  # A0
            raise cv.Invalid("ESP8266: Only pin A0 (GPIO17) supports ADC")
        return pins.gpio_pin_schema(
            {CONF_ANALOG: True, CONF_INPUT: True}, internal=True
        )(value)

    if CORE.is_rp2040:
        value = pins.internal_gpio_input_pin_number(value)
        if value not in (26, 27, 28, 29):
            raise cv.Invalid("RP2040: Only pins 26, 27, 28 and 29 support ADC")
        return pins.internal_gpio_input_pin_schema(value)

    if CORE.is_libretiny:
        return pins.gpio_pin_schema(
            {CONF_ANALOG: True, CONF_INPUT: True}, internal=True
        )(value)

    raise NotImplementedError
