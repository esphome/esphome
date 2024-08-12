import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MODE,
    CONF_ROTATION,
    CONF_VALUE,
)
from esphome.cpp_types import nullptr

from ..defines import (
    ARC_MODES,
    CONF_ADJUSTABLE,
    CONF_CHANGE_RATE,
    CONF_END_ANGLE,
    CONF_INDICATOR,
    CONF_KNOB,
    CONF_MAIN,
    CONF_START_ANGLE,
    literal,
)
from ..lv_validation import angle, get_start_value, lv_float
from ..lvcode import lv, lv_obj
from ..types import LvNumber, NumberType
from . import Widget

CONF_ARC = "arc"
ARC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_MIN_VALUE, default=0): cv.int_,
        cv.Optional(CONF_MAX_VALUE, default=100): cv.int_,
        cv.Optional(CONF_START_ANGLE, default=135): angle,
        cv.Optional(CONF_END_ANGLE, default=45): angle,
        cv.Optional(CONF_ROTATION, default=0.0): angle,
        cv.Optional(CONF_ADJUSTABLE, default=False): bool,
        cv.Optional(CONF_MODE, default="NORMAL"): ARC_MODES.one_of,
        cv.Optional(CONF_CHANGE_RATE, default=720): cv.uint16_t,
    }
)

ARC_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_float,
    }
)


class ArcType(NumberType):
    def __init__(self):
        super().__init__(
            CONF_ARC,
            LvNumber("lv_arc_t"),
            parts=(CONF_MAIN, CONF_INDICATOR, CONF_KNOB),
            schema=ARC_SCHEMA,
            modify_schema=ARC_MODIFY_SCHEMA,
        )

    async def to_code(self, w: Widget, config):
        if CONF_MIN_VALUE in config:
            lv.arc_set_range(w.obj, config[CONF_MIN_VALUE], config[CONF_MAX_VALUE])
            lv.arc_set_bg_angles(
                w.obj, config[CONF_START_ANGLE] // 10, config[CONF_END_ANGLE] // 10
            )
            lv.arc_set_rotation(w.obj, config[CONF_ROTATION] // 10)
            lv.arc_set_mode(w.obj, literal(config[CONF_MODE]))
            lv.arc_set_change_rate(w.obj, config[CONF_CHANGE_RATE])

        if config.get(CONF_ADJUSTABLE) is False:
            lv_obj.remove_style(w.obj, nullptr, literal("LV_PART_KNOB"))
            w.clear_flag("LV_OBJ_FLAG_CLICKABLE")

        value = await get_start_value(config)
        if value is not None:
            lv.arc_set_value(w.obj, value)


arc_spec = ArcType()
