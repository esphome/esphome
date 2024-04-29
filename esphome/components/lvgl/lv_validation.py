import esphome.config_validation as cv
from esphome.schema_extractors import (
    SCHEMA_EXTRACT,
    schema_extractor,
)
from esphome.components.font import Font
from esphome.components.color import ColorStruct
from .defines import (
    LV_FONTS,
    CONF_IMG,
    CONF_ROTARY_ENCODERS,
    CONF_TOUCHSCREENS,
    LvConstant,
)
from ...core import HexInt

lv_uses = {
    "USER_DATA",
    "LOG",
    "STYLE",
    "FONT_PLACEHOLDER",
    "THEME_DEFAULT",
}

lv_fonts_used = set()
esphome_fonts_used = set()

REQUIRED_COMPONENTS = {
    CONF_IMG: "image",
    CONF_ROTARY_ENCODERS: "rotary_encoder",
    CONF_TOUCHSCREENS: "touchscreen",
}
# List of other components used
lvgl_components_required = set()


def requires_component(comp):
    def validator(value):
        lvgl_components_required.add(comp)
        return cv.requires_component(comp)(value)

    return validator


@schema_extractor("one_of")
def color(value):
    if value == SCHEMA_EXTRACT:
        return ["hex color value", "color ID"]
    if isinstance(value, int):
        return value
    return cv.use_id(ColorStruct)(value)


def color_retmapper(value):
    if isinstance(value, cv.Lambda):
        return cv.returning_lambda(value)
    if isinstance(value, int):
        hexval = HexInt(value)
        return f"lv_color_hex({hexval})"
    lvgl_components_required.add("color")
    return f"lv_color_from({value})"


def lv_builtin_font(value):
    font = cv.one_of(*LV_FONTS, lower=True)(value)
    lv_fonts_used.add(font)
    return "&lv_font_" + font


def font(value):
    """Accept either the name of a built-in LVGL font, or the ID of an ESPHome font"""
    if value == SCHEMA_EXTRACT:
        return LV_FONTS
    if isinstance(value, str) and value.lower() in LV_FONTS:
        return lv_builtin_font(value)
    lv_uses.add("FONT")
    font = cv.use_id(Font)(value)
    esphome_fonts_used.add(font)
    lvgl_components_required.add("font")
    return cv.requires_component("font")(f"{font}_as_lv_font_")


def is_esphome_font(font):
    return "_as_lv_font_" in font


@schema_extractor("one_of")
def bool_(value):
    if value == SCHEMA_EXTRACT:
        return ["true", "false"]
    return "true" if cv.boolean(value) else "false"


def prefix(value, choices, prefix):
    if isinstance(value, str) and value.startswith(prefix):
        return cv.one_of(*list(map(lambda v: prefix + v, choices)), upper=True)(value)
    return prefix + cv.one_of(*choices, upper=True)(value)


def animated(value):
    if isinstance(value, bool):
        value = "ON" if value else "OFF"
    return one_of(LvConstant("LV_ANIM_", "OFF", "ON"))(value)


def key_code(value):
    value = cv.Any(cv.All(cv.string_strict, cv.Length(min=1, max=1)), cv.uint8_t)(value)
    if isinstance(value, str):
        return ord(value[0])
    return value


def one_of(consts: LvConstant):
    """Allow one of a list of choices, mapped to upper case, and prepend the choice with the prefix.
    It's also permitted to include the prefix in the value"""

    @schema_extractor("one_of")
    def validator(value):
        if value == SCHEMA_EXTRACT:
            return consts.choices
        return prefix(value, consts.choices, consts.prefix)

    return validator


def join_enums(enums, prefix=""):
    return "|".join(map(lambda e: f"(int){prefix}{e.upper()}", enums))


def lv_id_name(value):
    if value == SCHEMA_EXTRACT:
        return "id"
    return cv.validate_id_name(value)


def pixels_or_percent(value):
    """A length in one axis - either a number (pixels) or a percentage"""
    if value == SCHEMA_EXTRACT:
        return ["pixels", "..%"]
    if isinstance(value, int):
        return str(cv.int_(value))
    # Will throw an exception if not a percentage.
    return f"lv_pct({int(cv.percentage(value) * 100)})"


def lv_zoom(value):
    value = cv.float_range(0.1, 10.0)(value)
    return int(value * 256)


def lv_angle(value):
    """
    Validation for an angle in degrees, converted to an integer representing 0.1deg units
    :param value: The input in the range 0..360
    :return: An angle in 1/10 degree units.
    """
    return int(cv.float_range(0.0, 360.0)(cv.angle(value)) * 10)


def lv_pixels(value):
    """A size in one axis - one of "size_content", a number (pixels) or a percentage"""
    if isinstance(value, str) and value.lower().endswith("px"):
        return cv.int_(value[:-2])
    return str(cv.int_(value))


@schema_extractor("one_of")
def lv_size(value):
    """A size in one axis - one of "size_content", a number (pixels) or a percentage"""
    if value == SCHEMA_EXTRACT:
        return ["size_content", "pixels", "..%"]
    if isinstance(value, str) and value.lower().endswith("px"):
        value = cv.int_(value[:-2])
    if isinstance(value, str) and not value.endswith("%"):
        if value.upper() == "SIZE_CONTENT":
            return "LV_SIZE_CONTENT"
        raise cv.Invalid("must be 'size_content', a pixel position or a percentage")
    if isinstance(value, int):
        return str(cv.int_(value))
    # Will throw an exception if not a percentage.
    return f"lv_pct({int(cv.percentage(value) * 100)})"


@schema_extractor("one_of")
def opacity(value):
    if value == SCHEMA_EXTRACT:
        return ["TRANSP", "COVER", "..%"]
    value = cv.Any(cv.percentage, one_of(LvConstant("LV_OPA_", "TRANSP", "COVER")))(
        value
    )
    if isinstance(value, str):
        return value
    return int(value * 255)


def lv_stop_value(value):
    return cv.int_range(0, 255)(value)


def optional_boolean(value):
    if value is None:
        return True
    return cv.boolean(value)


def cv_int_list(il):
    il = cv.string(il)
    nl = il.replace(" ", "").split(",")
    return list(map(int, nl))


def lv_option_string(value):
    value = cv.string(value).strip()
    if value.find("\n") != -1:
        raise cv.Invalid("Options strings must not contain newlines")
    return value
