import esphome.config_validation as cv
from esphome.const import (
    CONF_GROUP,
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
    LV_OBJ_FLAG,
    LV_PART,
)
from ..lv_validation import (
    get_start_value,
    lv_angle_degrees,
    lv_float,
    lv_int,
    lv_positive_int,
)
from ..lvcode import lv, lv_expr, lv_obj
from ..types import LvNumber
from . import NumberType, Widget

CONF_ARC = "arc"
ARC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_MIN_VALUE, default=0): lv_int,
        cv.Optional(CONF_MAX_VALUE, default=100): lv_int,
        cv.Optional(CONF_START_ANGLE, default=135): lv_angle_degrees,
        cv.Optional(CONF_END_ANGLE, default=45): lv_angle_degrees,
        cv.Optional(CONF_ROTATION, default=0.0): lv_angle_degrees,
        cv.Optional(CONF_ADJUSTABLE, default=False): bool,
        cv.Optional(CONF_MODE, default="NORMAL"): ARC_MODES.one_of,
        cv.Optional(CONF_CHANGE_RATE, default=720): lv_positive_int,
    }
)

ARC_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VALUE): lv_float,
        cv.Optional(CONF_MIN_VALUE): lv_int,
        cv.Optional(CONF_MAX_VALUE): lv_int,
        cv.Optional(CONF_START_ANGLE): lv_angle_degrees,
        cv.Optional(CONF_END_ANGLE): lv_angle_degrees,
        cv.Optional(CONF_ROTATION): lv_angle_degrees,
        cv.Optional(CONF_MODE): ARC_MODES.one_of,
        cv.Optional(CONF_CHANGE_RATE): lv_positive_int,
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
        for prop, validator in ARC_MODIFY_SCHEMA.schema.items():
            if prop != CONF_VALUE:
                # start_angle and end_angle are mapped to bg_start_angle and bg_end_angle
                prop = str(prop)
                if prop.endswith("_angle"):
                    prop = "bg_" + prop
                await w.set_property(prop, config, processor=validator)
        if CONF_ADJUSTABLE in config:
            if not config[CONF_ADJUSTABLE]:
                lv_obj.remove_style(w.obj, nullptr, LV_PART.KNOB)
                w.clear_flag(LV_OBJ_FLAG.CLICKABLE)
            elif CONF_GROUP not in config:
                # For some reason arc does not get automatically added to the default group
                lv.group_add_obj(lv_expr.group_get_default(), w.obj)

        await w.set_property(CONF_VALUE, await get_start_value(config))


arc_spec = ArcType()
