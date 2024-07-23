import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MODE,
    CONF_ROTATION,
    CONF_VALUE,
)

from .defines import (
    ARC_MODES,
    CONF_ADJUSTABLE,
    CONF_ARC,
    CONF_CHANGE_RATE,
    CONF_END_ANGLE,
    CONF_START_ANGLE,
)
from .lv_validation import angle, get_start_value, lv_float
from .types import lv_arc_t
from .widget import Widget, WidgetType

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


class ArcType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_ARC,
            ARC_SCHEMA,
            ARC_MODIFY_SCHEMA,
        )

    @property
    def w_type(self):
        return lv_arc_t

    async def to_code(self, w: Widget, config):
        var = w.obj
        init = []
        if CONF_MIN_VALUE in config:
            init.extend(
                [
                    f"lv_arc_set_range({var}, {config[CONF_MIN_VALUE]}, {config[CONF_MAX_VALUE]})",
                    f"lv_arc_set_bg_angles({var}, {config[CONF_START_ANGLE] // 10}, {config[CONF_END_ANGLE] // 10})",
                    f"lv_arc_set_rotation({var}, {config[CONF_ROTATION] // 10})",
                    f"lv_arc_set_mode({var}, {config[CONF_MODE]})",
                    f"lv_arc_set_change_rate({var}, {config[CONF_CHANGE_RATE]})",
                ]
            )
        if config.get(CONF_ADJUSTABLE) is False:
            init.extend(
                [
                    f"lv_obj_remove_style({var}, nullptr, LV_PART_KNOB)",
                    f"lv_obj_clear_flag({var}, LV_OBJ_FLAG_CLICKABLE)",
                ]
            )

        value = await get_start_value(config)
        if value is not None:
            init.append(f"lv_arc_set_value({var}, {value})")
        return init


arc_spec = ArcType()
