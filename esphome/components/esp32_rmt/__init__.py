from esphome.components.esp32 import get_esp32_variant
from esphome.components.esp32.const import VARIANT_ESP32H2
import esphome.config_validation as cv

CODEOWNERS = ["@jesserockz"]


def validate_clock_resolution():
    def _validator(value):
        cv.only_on_esp32(value)
        value = cv.int_(value)
        variant = get_esp32_variant()
        if variant == VARIANT_ESP32H2 and value > 32000000:
            raise cv.Invalid(
                f"ESP32 variant {variant} has a max clock_resolution of 32000000."
            )
        if value > 80000000:
            raise cv.Invalid(
                f"ESP32 variant {variant} has a max clock_resolution of 80000000."
            )
        return value

    return _validator
