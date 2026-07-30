from esphome.components import esp32
import esphome.config_validation as cv
from esphome.core import CORE

CODEOWNERS = ["@jesserockz"]

VARIANTS_NO_RMT = {esp32.VARIANT_ESP32C2, esp32.VARIANT_ESP32C61}


def validate_rmt_not_supported(rmt_only_keys):
    """Validate that RMT-only config keys are not used on variants without RMT hardware."""
    rmt_only_keys = set(rmt_only_keys)

    def _validator(config):
        if CORE.is_esp32:
            variant = esp32.get_esp32_variant()
            if variant in VARIANTS_NO_RMT:
                unsupported = rmt_only_keys.intersection(config)
                if unsupported:
                    keys = ", ".join(sorted(f"'{k}'" for k in unsupported))
                    raise cv.Invalid(
                        f"{keys} not available on {variant} (no RMT hardware)"
                    )
        return config

    return _validator


def validate_clock_resolution():
    def _validator(value):
        cv.only_on_esp32(value)
        value = cv.int_(value)
        variant = esp32.get_esp32_variant()
        if variant == esp32.VARIANT_ESP32H2 and value > 32000000:
            raise cv.Invalid(
                f"ESP32 variant {variant} has a max clock_resolution of 32000000."
            )
        if value > 80000000:
            raise cv.Invalid(
                f"ESP32 variant {variant} has a max clock_resolution of 80000000."
            )
        return value

    return _validator
