import esphome.config_validation as cv
from esphome.const import CONF_MAX_VALUE, CONF_MIN_VALUE, CONF_MODE, CONF_VALUE

from ..defines import (
    BAR_MODES,
    CONF_ANIMATED,
    CONF_INDICATOR,
    CONF_MAIN,
    CONF_START_VALUE,
    literal,
)
from ..lv_validation import animated, lv_int
from ..lvcode import lv
from ..types import LvNumber, NumberType
from . import Widget

# Note this file cannot be called "bar.py" because that name is disallowed.

CONF_BAR = "bar"


def validate_bar(config):
    if config.get(CONF_MODE) != "LV_BAR_MODE_RANGE" and CONF_START_VALUE in config:
        raise cv.Invalid(
            f"{CONF_START_VALUE} is only allowed when {CONF_MODE} is set to 'RANGE'"
        )
    if (CONF_MIN_VALUE in config) != (CONF_MAX_VALUE in config):
        raise cv.Invalid(
            f"If either {CONF_MIN_VALUE} or {CONF_MAX_VALUE} is set, both must be set"
        )
    return config


BAR_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_int,
        cv.Optional(CONF_START_VALUE): lv_int,
        cv.Optional(CONF_MIN_VALUE): lv_int,
        cv.Optional(CONF_MAX_VALUE): lv_int,
        cv.Optional(CONF_MODE): BAR_MODES.one_of,
        cv.Optional(CONF_ANIMATED, default=True): animated,
    }
).add_extra(validate_bar)


class BarType(NumberType):
    def __init__(self):
        super().__init__(
            CONF_BAR,
            LvNumber("lv_bar_t"),
            parts=(CONF_MAIN, CONF_INDICATOR),
            schema=BAR_SCHEMA,
        )

    async def to_code(self, w: Widget, config):
        var = w.obj
        if mode := config.get(CONF_MODE):
            lv.bar_set_mode(var, literal(mode))
        is_animated = literal(config[CONF_ANIMATED])
        if CONF_MIN_VALUE in config:
            lv.bar_set_range(
                var,
                await lv_int.process(config[CONF_MIN_VALUE]),
                await lv_int.process(config[CONF_MAX_VALUE]),
            )
        if value := await lv_int.process(config.get(CONF_VALUE)):
            lv.bar_set_value(var, value, is_animated)
        if start_value := await lv_int.process(config.get(CONF_START_VALUE)):
            lv.bar_set_start_value(var, start_value, is_animated)

    @property
    def animated(self):
        return True


bar_spec = BarType()
