import esphome.codegen as cg
from esphome.components.display_menu_base import CONF_LABEL
import esphome.config_validation as cv
from esphome.const import CONF_COLOR, CONF_HEIGHT, CONF_WIDTH

from .. import add_lv_use
from ..defines import CONF_MAIN
from ..lv_validation import lv_color, size
from ..lvcode import lv_add
from ..types import LvCompound, LvType
from . import WidgetType
from .lv_bar import CONF_BAR
from .slider import CONF_SLIDER

# esphome::Color, not lv_color_t: it carries the same value but exposes the components as
# `x.r`/`x.g`/`x.b`, and converts to lv_color_t on its own where LVGL needs one.
Color = cg.esphome_ns.class_("Color")

lv_color_picker_t = LvType(
    "LvColorPickerType",
    parents=(LvCompound,),
    largs=[(Color, "x")],
    lvalue=lambda w: w.var.get_color(),
    has_on_value=True,
)

# Makes `lvgl.widget.update` with a `color:` report the change through `on_value`, the same
# as setting the value of any other widget does.
lv_color_picker_t.value_property = CONF_COLOR

CONF_COLOR_PICKER = "color_picker"

COLOR_PICKER_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_COLOR): lv_color,
    }
)

COLOR_PICKER_SCHEMA = COLOR_PICKER_MODIFY_SCHEMA.extend(
    {
        cv.Required(CONF_WIDTH): size,
        cv.Optional(CONF_HEIGHT): cv.invalid("Height will be set to the same as width"),
    }
)


class ColorPickerType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_COLOR_PICKER,
            lv_color_picker_t,
            parts=(CONF_MAIN,),
            schema=COLOR_PICKER_SCHEMA,
            modify_schema=COLOR_PICKER_MODIFY_SCHEMA,
            lv_name="obj",
        )

    def validate(self, value):
        add_lv_use(CONF_COLOR_PICKER)
        return super().validate(value)

    async def to_code(self, w, config: dict):
        if color := config.get(CONF_COLOR):
            lv_add(w.var.set_color(await lv_color.process(color)))
        # The widget is square, so the height simply follows the configured width.
        # SIZE_CONTENT works too: the widget reports a size based on its text font.
        # Width is required when creating the widget but absent when updating one.
        if (width := config.get(CONF_WIDTH)) is not None:
            w.set_style(CONF_HEIGHT, await size.process(width), 0)

    def get_uses(self):
        return ("flex", CONF_SLIDER, CONF_BAR, CONF_LABEL)


color_picker_spec = ColorPickerType()
