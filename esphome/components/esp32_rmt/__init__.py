import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE

CODEOWNERS = ["@jesserockz"]

RMT_TX_CHANNELS = {
    esp32.const.VARIANT_ESP32: [0, 1, 2, 3, 4, 5, 6, 7],
    esp32.const.VARIANT_ESP32S2: [0, 1, 2, 3],
    esp32.const.VARIANT_ESP32S3: [0, 1, 2, 3],
    esp32.const.VARIANT_ESP32C3: [0, 1],
    esp32.const.VARIANT_ESP32C6: [0, 1],
    esp32.const.VARIANT_ESP32H2: [0, 1],
}

RMT_RX_CHANNELS = {
    esp32.const.VARIANT_ESP32: [0, 1, 2, 3, 4, 5, 6, 7],
    esp32.const.VARIANT_ESP32S2: [0, 1, 2, 3],
    esp32.const.VARIANT_ESP32S3: [4, 5, 6, 7],
    esp32.const.VARIANT_ESP32C3: [2, 3],
    esp32.const.VARIANT_ESP32C6: [2, 3],
    esp32.const.VARIANT_ESP32H2: [2, 3],
}

rmt_channel_t = cg.global_ns.enum("rmt_channel_t")
RMT_CHANNEL_ENUMS = {
    0: rmt_channel_t.RMT_CHANNEL_0,
    1: rmt_channel_t.RMT_CHANNEL_1,
    2: rmt_channel_t.RMT_CHANNEL_2,
    3: rmt_channel_t.RMT_CHANNEL_3,
    4: rmt_channel_t.RMT_CHANNEL_4,
    5: rmt_channel_t.RMT_CHANNEL_5,
    6: rmt_channel_t.RMT_CHANNEL_6,
    7: rmt_channel_t.RMT_CHANNEL_7,
}


def use_new_rmt_driver():
    framework_version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    if CORE.using_esp_idf and framework_version >= cv.Version(5, 0, 0):
        return True
    return False


def validate_clock_resolution():
    def _validator(value):
        cv.only_on_esp32(value)
        value = cv.int_(value)
        variant = esp32.get_esp32_variant()
        if variant == esp32.const.VARIANT_ESP32H2 and value > 32000000:
            raise cv.Invalid(
                f"ESP32 variant {variant} has a max clock_resolution of 32000000."
            )
        if value > 80000000:
            raise cv.Invalid(
                f"ESP32 variant {variant} has a max clock_resolution of 80000000."
            )
        return value

    return _validator


def validate_rmt_channel(*, tx: bool):
    rmt_channels = RMT_TX_CHANNELS if tx else RMT_RX_CHANNELS

    def _validator(value):
        cv.only_on_esp32(value)
        value = cv.int_(value)
        variant = esp32.get_esp32_variant()
        if variant not in rmt_channels:
            raise cv.Invalid(f"ESP32 variant {variant} does not support RMT.")
        if value not in rmt_channels[variant]:
            raise cv.Invalid(
                f"RMT channel {value} does not support {'transmitting' if tx else 'receiving'} for ESP32 variant {variant}."
            )
        return cv.enum(RMT_CHANNEL_ENUMS)(value)

    return _validator
