import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.components import esp32

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
