import esphome.codegen as cg
from esphome.components.lvgl.schemas import STYLE_PROPS
import esphome.config_validation as cv
from esphome.const import (
    CONF_ITEMS,
    CONF_MODE,
    CONF_RANGE_FROM,
    CONF_RANGE_TO,
    CONF_ROTATION,
)

from ..defines import (
    CONF_ANGLE_RANGE,
    CONF_INDICATOR,
    CONF_MAIN,
    CONF_RADIUS,
    CONF_STYLE_ID,
    LV_SCALE_MODE,
    literal,
)
from ..lv_validation import lv_bool
from ..lvcode import lv, lv_expr
from ..schemas import STYLE_SCHEMA
from ..styles import create_style, has_style_props
from ..types import LvType, WidgetType, lv_style_t
from . import Widget

lv_scale_t = LvType("lv_scale_t")
lv_scale_section_t = LvType("lv_scale_section_t")

CONF_SCALE = "scale"
CONF_DRAW_TICKS_ON_TOP = "draw_ticks_on_top"
CONF_LABEL_SHOW = "label_show"
CONF_TOTAL_TICK_COUNT = "total_tick_count"
CONF_MAJOR_TICK_EVERY = "major_tick_every"
CONF_SECTIONS = "sections"
CONF_SECTION_ID = "section_id"

STYLE_BASE = cv.Schema(
    {cv.GenerateID(CONF_STYLE_ID): cv.declare_id(lv_style_t)}
).extend(STYLE_SCHEMA)

SCALE_STYLE_SCHEMA = STYLE_BASE.extend(
    {
        cv.Optional(CONF_MAIN): STYLE_BASE,
        cv.Optional(CONF_ITEMS): STYLE_BASE,
        cv.Optional(CONF_INDICATOR): STYLE_BASE,
    }
)

SECTION_SCHEMA = SCALE_STYLE_SCHEMA.extend(
    {
        cv.GenerateID(CONF_SECTION_ID): cv.declare_id(lv_scale_section_t),
        cv.Required(CONF_RANGE_FROM): cv.float_,
        cv.Required(CONF_RANGE_TO): cv.float_,
    }
)

LINE_STYLE = cv.Schema(
    {cv.Optional(k): STYLE_PROPS[k] for k in ("line_color", "line_width", "line_opa")}
)

# Restricted sets of styles for each mode
MODE_STYLE_SCHEMAS = {
    "HORIZONTAL_TOP": SCALE_STYLE_SCHEMA,
    "HORIZONTAL_BOTTOM": SCALE_STYLE_SCHEMA,
    "VERTICAL_LEFT": SCALE_STYLE_SCHEMA,
    "VERTICAL_RIGHT": SCALE_STYLE_SCHEMA,
    "ROUND_INNER": SCALE_STYLE_SCHEMA,
    "ROUND_OUTER": SCALE_STYLE_SCHEMA,
}


def mode_check(config):
    if CONF_RADIUS not in config and "ROUND" in config[CONF_MODE]:
        config[CONF_RADIUS] = "LV_RADIUS_CIRCLE"
    return config


SCALE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_RANGE_FROM, default=0.0): cv.float_,
        cv.Optional(CONF_RANGE_TO, default=100.0): cv.float_,
        cv.Optional(CONF_ANGLE_RANGE): cv.int_range(0, 360),
        cv.Optional(CONF_ROTATION): cv.int_range(0, 360),
        cv.Optional(CONF_DRAW_TICKS_ON_TOP, default=False): lv_bool,
        cv.Optional(CONF_LABEL_SHOW, default=True): lv_bool,
        cv.Optional(CONF_TOTAL_TICK_COUNT, default=50): cv.int_range(1, 1000),
        cv.Optional(CONF_MAJOR_TICK_EVERY, default=10): cv.int_range(1, 1000),
        cv.Optional(CONF_MODE, default="HORIZONTAL_TOP"): LV_SCALE_MODE.one_of,
        cv.Optional(CONF_SECTIONS): cv.ensure_list(SECTION_SCHEMA),
    }
).add_extra(mode_check)


class ScaleType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_SCALE,
            lv_scale_t,
            (CONF_MAIN, CONF_ITEMS, CONF_INDICATOR),
            SCALE_SCHEMA,
        )


class SectionType(WidgetType):
    def __init__(self):
        super().__init__(
            "section",
            lv_scale_section_t,
            (CONF_MAIN, CONF_ITEMS, CONF_INDICATOR),
            is_mock=True,
        )

    async def to_code(self, w: Widget, config):
        for prop in (
            CONF_ANGLE_RANGE,
            CONF_ROTATION,
            CONF_DRAW_TICKS_ON_TOP,
            CONF_LABEL_SHOW,
            CONF_TOTAL_TICK_COUNT,
            CONF_MAJOR_TICK_EVERY,
            CONF_MODE,
        ):
            await w.set_property(prop, config)
        lv.scale_set_range(w.obj, config[CONF_RANGE_FROM], config[CONF_RANGE_TO])
        for section in config.get(CONF_SECTIONS, ()):
            svar = cg.Pvariable(
                section[CONF_SECTION_ID], lv_expr.scale_add_section(w.obj)
            )
            lv.scale_section_set_range(
                svar, section[CONF_RANGE_FROM], section[CONF_RANGE_TO]
            )
            if has_style_props(section):
                sstyle = await create_style(section[CONF_STYLE_ID], section)
                lv.scale_section_set_style(svar, literal("LV_PART_MAIN"), sstyle)
            if items := section.get(CONF_ITEMS):
                sstyle = await create_style(section[CONF_STYLE_ID], items)
                lv.scale_section_set_style(svar, literal("LV_PART_ITEMS"), sstyle)
            if indicator := section.get(CONF_INDICATOR):
                sstyle = await create_style(section[CONF_STYLE_ID], indicator)
                lv.scale_section_set_style(svar, literal("LV_PART_INDICATOR"), sstyle)


scale_spec = ScaleType()
section_spec = SectionType()
