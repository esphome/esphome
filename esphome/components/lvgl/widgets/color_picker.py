from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_COLOR, CONF_HEIGHT, CONF_ITEMS, CONF_WIDTH
from esphome.core import CORE

from ..defines import (
    CONF_COLOR_PICKER,
    CONF_EXT_CLICK_AREA,
    CONF_KNOB,
    CONF_MAIN,
    CONF_STYLES,
    CONF_THEME,
    DOMAIN,
    add_define,
    add_lv_use,
    literal,
)
from ..lv_validation import lv_color, size
from ..lvcode import lv_add
from ..types import LvCompound, LvType
from . import Widget, WidgetType
from .label import CONF_LABEL
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
CONF_WHEEL = "wheel"

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

# The wheel draws hue and saturation itself, and the layout drops the RGB sliders to make
# room for it, leaving brightness as the only slider that still fits.
WHEEL_EXCLUDED_SLIDERS = ("hue", "saturation", "red", "green", "blue")


def validate_sliders(value: Any) -> list[str]:
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
        cv.Optional(CONF_WIDTH, default="SIZE_CONTENT"): size,
        cv.Optional(CONF_HEIGHT): cv.invalid("Height will be set to the same as width"),
        cv.Optional(CONF_SLIDERS, default=list(SLIDER_NAMES)): validate_sliders,
        cv.Optional(CONF_WHEEL, default=False): cv.boolean,
    }
)


# Starting colour for a picker with no way to change saturation, which would otherwise be stuck
# at the grey it starts with. Saturation changes with its own slider, the wheel, or any of the
# red, green and blue sliders, so only these two sliders on their own leave it fixed.
SATURATED_DEFAULT_COLOR = 0xFF0000
HUE_AND_BRIGHTNESS = {"hue", "brightness"}


def _knob_color_is_set(knob: dict) -> bool:
    """Whether a knob is given a background colour, directly or through named styles."""
    return CONF_BG_COLOR in knob or bool(knob.get(CONF_STYLES))


def _theme_sets_knob_color() -> bool:
    """Whether any LVGL theme gives the color picker's knobs a background colour."""
    configs = CORE.config.get(DOMAIN) or []
    if isinstance(configs, dict):
        configs = [configs]
    return any(
        _knob_color_is_set(
            ((config.get(CONF_THEME) or {}).get(CONF_COLOR_PICKER) or {}).get(
                CONF_KNOB, {}
            )
        )
        for config in configs
    )


def _sliders(config: dict) -> list[str]:
    """Get the sliders a widget was created with, in the widget's own order."""
    return config.get(CONF_SLIDERS) or list(SLIDER_NAMES)


class ColorPickerType(WidgetType):
    def __init__(self) -> None:
        super().__init__(
            CONF_COLOR_PICKER,
            lv_color_picker_t,
            parts=(CONF_MAIN, CONF_ITEMS, CONF_KNOB),
            schema=COLOR_PICKER_SCHEMA,
            modify_schema=COLOR_PICKER_MODIFY_SCHEMA,
            lv_name="obj",
        )

    def validate(self, value: Any) -> Any:
        add_lv_use(CONF_COLOR_PICKER)
        value = dict(value)
        if cv.boolean(value.get(CONF_WHEEL, False)):
            # The wheel is a conical hue gradient under a radial white-to-transparent one;
            # LVGL's software renderer only draws either kind when this is enabled.
            add_define("LV_USE_DRAW_SW_COMPLEX_GRADIENTS")
            if CONF_SLIDERS in value:
                sliders = validate_sliders(value[CONF_SLIDERS])
                if any(name in sliders for name in WHEEL_EXCLUDED_SLIDERS):
                    raise cv.Invalid(
                        "the color wheel already shows hue and saturation, and replaces "
                        "the red, green and blue sliders with it; remove them from "
                        "'sliders' or set 'wheel: false'"
                    )
            else:
                # Otherwise the schema default would build every slider including these.
                value[CONF_SLIDERS] = [
                    name for name in SLIDER_NAMES if name not in WHEEL_EXCLUDED_SLIDERS
                ]
        return super().validate(value)

    async def get_ctor_args(self, config: dict) -> list:
        # The layout is worked out from the sliders, so they are needed before the widget is
        # built rather than set afterwards.
        return [
            literal(
                " | ".join(
                    f"{lv_color_picker_t}::SLIDER_FLAG_{name.upper()}"
                    for name in _sliders(config)
                )
            ),
            config[CONF_WHEEL],
        ]

    @staticmethod
    def _sliders_of(w: Widget) -> list[Widget]:
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

    def obj_targets(self, w: Widget, prop: str) -> list[Widget]:
        # Nothing is ever pressed on its own object, so the touch margin belongs on the
        # sliders. Everything else, scrolling in particular, is a matter for the container
        # holding them and so stays where it is.
        if prop == CONF_EXT_CLICK_AREA:
            return self._sliders_of(w)
        return [w]

    def part_targets(self, w: Widget, part: str) -> list[tuple[Widget, str]]:
        # The widget is made of sliders, so `items` and `knob` are the main and knob parts of
        # each of those. Its own object is a plain container with neither. Only the sliders
        # the widget was created with exist, so only those are styled.
        if part == CONF_MAIN:
            return [(w, part)]
        target_part = CONF_MAIN if part == CONF_ITEMS else part
        return [(slider, target_part) for slider in self._sliders_of(w)]

    async def to_code(self, w: Widget, config: dict) -> None:
        # Only the configuration that creates the widget has `sliders`; an update does not.
        creating = CONF_SLIDERS in config
        if creating and config[CONF_WHEEL]:
            # Without a wheel anywhere, its code and gradients are left out of the build.
            cg.add_define("USE_LVGL_COLOR_PICKER_WHEEL")
        color = config.get(CONF_COLOR)
        if (
            color is None
            and creating
            and not config[CONF_WHEEL]
            and set(config[CONF_SLIDERS]) <= HUE_AND_BRIGHTNESS
        ):
            color = SATURATED_DEFAULT_COLOR
        # A hex colour validates to a plain int, so black must not be mistaken for absent.
        if color is not None:
            lv_add(w.var.set_color(await lv_color.process(color)))
        # Each knob is normally tinted with the colour its own slider shows. That is applied
        # as a local style, which outranks any style the knob has been given, so a background
        # colour configured for the knob, here or in the theme, would never show; tinting
        # gives way to it instead.
        if _knob_color_is_set(config.get(CONF_KNOB, {})) or (
            creating and _theme_sets_knob_color()
        ):
            lv_add(w.var.set_tint_knobs(False))
        # The widget is square, so the height simply follows the configured width.
        # SIZE_CONTENT works too: the widget reports a size based on its text font.
        # Width is required when creating the widget but absent when updating one.
        if (width := config.get(CONF_WIDTH)) is not None:
            w.set_style(CONF_HEIGHT, await size.process(width), 0)

    def get_uses(self) -> tuple[str, ...]:
        return ("flex", CONF_SLIDER, CONF_BAR, CONF_LABEL)


color_picker_spec = ColorPickerType()
