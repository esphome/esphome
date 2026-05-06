from esphome import pins
import esphome.codegen as cg
from esphome.components.esp32 import (
    VARIANT_ESP32,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32H2,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    get_esp32_variant,
)
import esphome.config_validation as cv
from esphome.const import CONF_ANALOG, CONF_INPUT, CONF_NUMBER, PLATFORM_ESP8266
from esphome.core import CORE

CODEOWNERS = ["@esphome/core"]

adc_ns = cg.esphome_ns.namespace("adc")


"""
From the below patch versions (and 5.2+) ADC_ATTEN_DB_11 is deprecated and replaced with ADC_ATTEN_DB_12.
4.4.7
5.0.5
5.1.3
5.2+
"""

ATTENUATION_MODES = {
    "0db": cg.global_ns.ADC_ATTEN_DB_0,
    "2.5db": cg.global_ns.ADC_ATTEN_DB_2_5,
    "6db": cg.global_ns.ADC_ATTEN_DB_6,
    "11db": adc_ns.ADC_ATTEN_DB_12_COMPAT,
    "12db": adc_ns.ADC_ATTEN_DB_12_COMPAT,
    "auto": "auto",
}

sampling_mode = adc_ns.enum("SamplingMode", is_class=True)

SAMPLING_MODES = {
    "avg": sampling_mode.AVG,
    "min": sampling_mode.MIN,
    "max": sampling_mode.MAX,
}

adc_unit_t = cg.global_ns.enum("adc_unit_t", is_class=True)

adc_channel_t = cg.global_ns.enum("adc_channel_t", is_class=True)

# pin to adc1 channel mapping
# https://github.com/espressif/esp-idf/blob/v4.4.8/components/driver/include/driver/adc.h
ESP32_VARIANT_ADC1_PIN_TO_CHANNEL = {
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32/include/soc/adc_channel.h
    VARIANT_ESP32: {
        36: adc_channel_t.ADC_CHANNEL_0,
        37: adc_channel_t.ADC_CHANNEL_1,
        38: adc_channel_t.ADC_CHANNEL_2,
        39: adc_channel_t.ADC_CHANNEL_3,
        32: adc_channel_t.ADC_CHANNEL_4,
        33: adc_channel_t.ADC_CHANNEL_5,
        34: adc_channel_t.ADC_CHANNEL_6,
        35: adc_channel_t.ADC_CHANNEL_7,
    },
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32c2/include/soc/adc_channel.h
    VARIANT_ESP32C2: {
        0: adc_channel_t.ADC_CHANNEL_0,
        1: adc_channel_t.ADC_CHANNEL_1,
        2: adc_channel_t.ADC_CHANNEL_2,
        3: adc_channel_t.ADC_CHANNEL_3,
        4: adc_channel_t.ADC_CHANNEL_4,
    },
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32c3/include/soc/adc_channel.h
    VARIANT_ESP32C3: {
        0: adc_channel_t.ADC_CHANNEL_0,
        1: adc_channel_t.ADC_CHANNEL_1,
        2: adc_channel_t.ADC_CHANNEL_2,
        3: adc_channel_t.ADC_CHANNEL_3,
        4: adc_channel_t.ADC_CHANNEL_4,
    },
    # ESP32-C5 ADC1 pin mapping - based on official ESP-IDF documentation
    # https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/api-reference/peripherals/gpio.html
    VARIANT_ESP32C5: {
        1: adc_channel_t.ADC_CHANNEL_0,
        2: adc_channel_t.ADC_CHANNEL_1,
        3: adc_channel_t.ADC_CHANNEL_2,
        4: adc_channel_t.ADC_CHANNEL_3,
        5: adc_channel_t.ADC_CHANNEL_4,
        6: adc_channel_t.ADC_CHANNEL_5,
    },
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32c6/include/soc/adc_channel.h
    VARIANT_ESP32C6: {
        0: adc_channel_t.ADC_CHANNEL_0,
        1: adc_channel_t.ADC_CHANNEL_1,
        2: adc_channel_t.ADC_CHANNEL_2,
        3: adc_channel_t.ADC_CHANNEL_3,
        4: adc_channel_t.ADC_CHANNEL_4,
        5: adc_channel_t.ADC_CHANNEL_5,
        6: adc_channel_t.ADC_CHANNEL_6,
    },
    # https://docs.espressif.com/projects/esp-idf/en/latest/esp32c61/api-reference/peripherals/gpio.html
    VARIANT_ESP32C61: {
        1: adc_channel_t.ADC_CHANNEL_0,
        3: adc_channel_t.ADC_CHANNEL_1,
        4: adc_channel_t.ADC_CHANNEL_2,
        5: adc_channel_t.ADC_CHANNEL_3,
    },
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32h2/include/soc/adc_channel.h
    VARIANT_ESP32H2: {
        1: adc_channel_t.ADC_CHANNEL_0,
        2: adc_channel_t.ADC_CHANNEL_1,
        3: adc_channel_t.ADC_CHANNEL_2,
        4: adc_channel_t.ADC_CHANNEL_3,
        5: adc_channel_t.ADC_CHANNEL_4,
    },
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32p4/include/soc/adc_channel.h
    VARIANT_ESP32P4: {
        16: adc_channel_t.ADC_CHANNEL_0,
        17: adc_channel_t.ADC_CHANNEL_1,
        18: adc_channel_t.ADC_CHANNEL_2,
        19: adc_channel_t.ADC_CHANNEL_3,
        20: adc_channel_t.ADC_CHANNEL_4,
        21: adc_channel_t.ADC_CHANNEL_5,
        22: adc_channel_t.ADC_CHANNEL_6,
        23: adc_channel_t.ADC_CHANNEL_7,
    },
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32s2/include/soc/adc_channel.h
    VARIANT_ESP32S2: {
        1: adc_channel_t.ADC_CHANNEL_0,
        2: adc_channel_t.ADC_CHANNEL_1,
        3: adc_channel_t.ADC_CHANNEL_2,
        4: adc_channel_t.ADC_CHANNEL_3,
        5: adc_channel_t.ADC_CHANNEL_4,
        6: adc_channel_t.ADC_CHANNEL_5,
        7: adc_channel_t.ADC_CHANNEL_6,
        8: adc_channel_t.ADC_CHANNEL_7,
        9: adc_channel_t.ADC_CHANNEL_8,
        10: adc_channel_t.ADC_CHANNEL_9,
    },
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32s3/include/soc/adc_channel.h
    VARIANT_ESP32S3: {
        1: adc_channel_t.ADC_CHANNEL_0,
        2: adc_channel_t.ADC_CHANNEL_1,
        3: adc_channel_t.ADC_CHANNEL_2,
        4: adc_channel_t.ADC_CHANNEL_3,
        5: adc_channel_t.ADC_CHANNEL_4,
        6: adc_channel_t.ADC_CHANNEL_5,
        7: adc_channel_t.ADC_CHANNEL_6,
        8: adc_channel_t.ADC_CHANNEL_7,
        9: adc_channel_t.ADC_CHANNEL_8,
        10: adc_channel_t.ADC_CHANNEL_9,
    },
}

# pin to adc2 channel mapping
# https://github.com/espressif/esp-idf/blob/v4.4.8/components/driver/include/driver/adc.h
ESP32_VARIANT_ADC2_PIN_TO_CHANNEL = {
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32/include/soc/adc_channel.h
    VARIANT_ESP32: {
        4: adc_channel_t.ADC_CHANNEL_0,
        0: adc_channel_t.ADC_CHANNEL_1,
        2: adc_channel_t.ADC_CHANNEL_2,
        15: adc_channel_t.ADC_CHANNEL_3,
        13: adc_channel_t.ADC_CHANNEL_4,
        12: adc_channel_t.ADC_CHANNEL_5,
        14: adc_channel_t.ADC_CHANNEL_6,
        27: adc_channel_t.ADC_CHANNEL_7,
        25: adc_channel_t.ADC_CHANNEL_8,
        26: adc_channel_t.ADC_CHANNEL_9,
    },
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32c2/include/soc/adc_channel.h
    VARIANT_ESP32C2: {
        5: adc_channel_t.ADC_CHANNEL_0,
    },
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32c3/include/soc/adc_channel.h
    VARIANT_ESP32C3: {
        5: adc_channel_t.ADC_CHANNEL_0,
    },
    # ESP32-C5 has no ADC2 channels
    VARIANT_ESP32C5: {},  # no ADC2
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32c6/include/soc/adc_channel.h
    VARIANT_ESP32C6: {},  # no ADC2
    # ESP32-C61 has no ADC2
    VARIANT_ESP32C61: {},  # no ADC2
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32h2/include/soc/adc_channel.h
    VARIANT_ESP32H2: {},  # no ADC2
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32p4/include/soc/adc_channel.h
    VARIANT_ESP32P4: {
        49: adc_channel_t.ADC_CHANNEL_0,
        50: adc_channel_t.ADC_CHANNEL_1,
        51: adc_channel_t.ADC_CHANNEL_2,
        52: adc_channel_t.ADC_CHANNEL_3,
        53: adc_channel_t.ADC_CHANNEL_4,
        54: adc_channel_t.ADC_CHANNEL_5,
    },
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32s2/include/soc/adc_channel.h
    VARIANT_ESP32S2: {
        11: adc_channel_t.ADC_CHANNEL_0,
        12: adc_channel_t.ADC_CHANNEL_1,
        13: adc_channel_t.ADC_CHANNEL_2,
        14: adc_channel_t.ADC_CHANNEL_3,
        15: adc_channel_t.ADC_CHANNEL_4,
        16: adc_channel_t.ADC_CHANNEL_5,
        17: adc_channel_t.ADC_CHANNEL_6,
        18: adc_channel_t.ADC_CHANNEL_7,
        19: adc_channel_t.ADC_CHANNEL_8,
        20: adc_channel_t.ADC_CHANNEL_9,
    },
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32s3/include/soc/adc_channel.h
    VARIANT_ESP32S3: {
        11: adc_channel_t.ADC_CHANNEL_0,
        12: adc_channel_t.ADC_CHANNEL_1,
        13: adc_channel_t.ADC_CHANNEL_2,
        14: adc_channel_t.ADC_CHANNEL_3,
        15: adc_channel_t.ADC_CHANNEL_4,
        16: adc_channel_t.ADC_CHANNEL_5,
        17: adc_channel_t.ADC_CHANNEL_6,
        18: adc_channel_t.ADC_CHANNEL_7,
        19: adc_channel_t.ADC_CHANNEL_8,
        20: adc_channel_t.ADC_CHANNEL_9,
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
        conf = pins.internal_gpio_input_pin_schema(value)
        value = conf[CONF_NUMBER]
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

        return conf

    if CORE.is_esp8266:
        conf = pins.gpio_pin_schema(
            {CONF_ANALOG: True, CONF_INPUT: True}, internal=True
        )(value)

        if conf[CONF_NUMBER] != 17:  # A0
            raise cv.Invalid("ESP8266: Only pin A0 (GPIO17) supports ADC")
        return conf

    if CORE.is_rp2040:
        conf = pins.internal_gpio_input_pin_schema(value)
        number = conf[CONF_NUMBER]
        if number not in (26, 27, 28, 29):
            raise cv.Invalid("RP2040: Only pins 26, 27, 28 and 29 support ADC")
        return conf

    if CORE.is_libretiny:
        return pins.gpio_pin_schema(
            {CONF_ANALOG: True, CONF_INPUT: True}, internal=True
        )(value)

    if CORE.is_nrf52:
        return pins.gpio_pin_schema(
            {CONF_ANALOG: True, CONF_INPUT: True}, internal=True
        )(value)

    raise NotImplementedError
