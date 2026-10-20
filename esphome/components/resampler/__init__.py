from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv

DOMAIN = "resampler"

resampler_ns = cg.esphome_ns.namespace("resampler")

CONF_TAPS = "taps"


def validate_taps(taps: Any) -> int:
    value = cv.int_range(min=16, max=128)(taps)
    if value % 4 != 0:
        raise cv.Invalid("Number of taps must be divisible by 4")
    return value
