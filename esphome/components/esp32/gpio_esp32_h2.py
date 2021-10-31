import esphome.config_validation as cv


def esp32_h2_validate_gpio_pin(value):
    # ESP32-H2 not yet supported
    raise cv.Invalid("ESP32-H2 isn't supported yet")


def esp32_h2_validate_supports(value):
    # ESP32-H2 not yet supported
    raise cv.Invalid("ESP32-H2 isn't supported yet")
