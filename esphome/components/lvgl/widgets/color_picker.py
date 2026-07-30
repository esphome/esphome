import esphome.codegen as cg
from esphome.components.display_menu_base import CONF_LABEL
import esphome.config_validation as cv
from esphome.const import CONF_COLOR, CONF_HEIGHT, CONF_ITEMS, CONF_WIDTH

from .. import add_lv_use
from ..defines import CONF_COLOR_PICKER, CONF_KNOB, CONF_MAIN, literal
from ..lv_validation import lv_color, size
from ..lvcode import lv_add
from ..types import LvCompound, LvType
from . import Widget, WidgetType
from .lv_bar import CONF_BAR
from .slider import CONF_SLIDER, slider_spec

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

CONF_SLIDERS = "sliders"

CONF_BG_COLOR = "bg_color"

# The sliders the widget can be built from. The order must match
# LvColorPickerType::SliderIndex, since it decides which bit of the mask each one gets.
SLIDER_NAMES = (
    "hue",
    "saturation",
    "brightness",
    "red",
    "green",
    "blue",
)

# Shorthand for the two sets of sliders that are usually wanted together.
SLIDER_GROUPS = {
    "hsv": ("hue", "saturation", "brightness"),
    "hsb": ("hue", "saturation", "brightness"),
    "rgb": ("red", "green", "blue"),
}


def validate_sliders(value):
    """Expand any group names and drop repeats, keeping the widget's own slider order."""
    value = cv.ensure_list(cv.one_of(*SLIDER_NAMES, *SLIDER_GROUPS, lower=True))(value)
    chosen = {name for item in value for name in SLIDER_GROUPS.get(item, (item,))}
    if not chosen:
        raise cv.Invalid("At least one slider is required")
    return [name for name in SLIDER_NAMES if name in chosen]


COLOR_PICKER_MODIFY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_COLOR): lv_color,
    }
)

COLOR_PICKER_SCHEMA = COLOR_PICKER_MODIFY_SCHEMA.extend(
    {
        cv.Required(CONF_WIDTH): size,
        cv.Optional(CONF_HEIGHT): cv.invalid("Height will be set to the same as width"),
        cv.Optional(CONF_SLIDERS, default=list(SLIDER_NAMES)): validate_sliders,
    }
)


def _sliders(config: dict) -> list[str]:
    """Get the sliders a widget was created with, in the widget's own order."""
    return config.get(CONF_SLIDERS) or list(SLIDER_NAMES)


class ColorPickerType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_COLOR_PICKER,
            lv_color_picker_t,
            parts=(CONF_MAIN, CONF_ITEMS, CONF_KNOB),
            schema=COLOR_PICKER_SCHEMA,
            modify_schema=COLOR_PICKER_MODIFY_SCHEMA,
            lv_name="obj",
        )

    def validate(self, value):
        add_lv_use(CONF_COLOR_PICKER)
        return super().validate(value)

    async def get_ctor_args(self, config: dict):
        # The layout is worked out from the sliders, so they are needed before the widget is
        # built rather than set afterwards.
        return [
            literal(
                " | ".join(
                    f"{lv_color_picker_t}::SLIDER_FLAG_{name.upper()}"
                    for name in _sliders(config)
                )
            )
        ]

    @staticmethod
    def _sliders_of(w) -> list[Widget]:
        """Wrap each slider the widget was created with, so it can be configured directly."""
        return [
            Widget(
                w.var.get_slider(
                    literal(f"{lv_color_picker_t}::SLIDER_{name.upper()}")
                ),
                slider_spec,
            )
            for name in _sliders(w.config or {})
        ]

    def obj_targets(self, w):
        # Its own object is a plain container that nothing is ever pressed on, so anything
        # to do with touch, such as `ext_click_area`, belongs on the sliders.
        return self._sliders_of(w)

    def part_targets(self, w, part):
        # The widget is made of sliders, so `items` and `knob` are the main and knob parts of
        # each of those. Its own object is a plain container with neither. Only the sliders
        # the widget was created with exist, so only those are styled.
        if part == CONF_MAIN:
            return [(w, part)]
        target_part = CONF_MAIN if part == CONF_ITEMS else part
        return [(slider, target_part) for slider in self._sliders_of(w)]

    async def to_code(self, w, config: dict):
        if color := config.get(CONF_COLOR):
            lv_add(w.var.set_color(await lv_color.process(color)))
        # Each knob is normally tinted with the colour its own slider shows, which would
        # overwrite a background colour configured for it, so that turns the tinting off.
        if CONF_BG_COLOR in config.get(CONF_KNOB, {}):
            lv_add(w.var.set_tint_knobs(False))
        # The widget is square, so the height simply follows the configured width.
        # SIZE_CONTENT works too: the widget reports a size based on its text font.
        # Width is required when creating the widget but absent when updating one.
        if (width := config.get(CONF_WIDTH)) is not None:
            w.set_style(CONF_HEIGHT, await size.process(width), 0)

    def get_uses(self):
        return ("flex", CONF_SLIDER, CONF_BAR, CONF_LABEL)


color_picker_spec = ColorPickerType()
