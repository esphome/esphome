import esphome.config_validation as cv

CONFIG_SCHEMA = cv.invalid(
    "The I2S audio media player has been removed. "
    "Use the speaker media player component instead. "
    "See https://esphome.io/components/media_player/speaker.html for details."
)
