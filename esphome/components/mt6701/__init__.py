import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@slimcdk"]

mt6701_ns = cg.esphome_ns.namespace("mt6701")
MT6701Component = mt6701_ns.class_("MT6701Component", cg.PollingComponent)

CONF_MT6701_ID = "mt6701_id"

# The configurable position registers (zero offset, analog start/stop) are
# 12-bit values covering one full revolution.
RESOLUTION_12BIT = 4096
MAX_POSITION_12BIT = RESOLUTION_12BIT - 1
ANGLE_TO_POSITION_12 = RESOLUTION_12BIT / 360


def _angle_to_position_12(value):
    value = cv.float_range(min=0, max=360)(
        cv.float_with_unit("angle", "(°|deg)")(value)
    )
    # The register cannot represent a full 360°, so clamp the top of the range
    # to the maximum count instead of letting it wrap back to 0.
    return min(round(value * ANGLE_TO_POSITION_12), MAX_POSITION_12BIT)


def _percent_to_position_12(value):
    value = cv.percentage(value)
    return min(round(value * RESOLUTION_12BIT), MAX_POSITION_12BIT)


def position12(value):
    """Validate a 12-bit position register value.

    Accepts a raw integer count (0-4095), an angle such as ``45deg`` or ``90°``,
    or a percentage of a full revolution such as ``25%``.
    """
    if isinstance(value, str) and value.endswith("%"):
        return _percent_to_position_12(value)
    if isinstance(value, str) and value.endswith(("°", "deg")):
        return _angle_to_position_12(value)
    return cv.int_range(min=0, max=MAX_POSITION_12BIT)(value)
