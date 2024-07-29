import esphome.codegen as cg
from esphome.components.binary_sensor import BinarySensor
from esphome.components.color import ColorStruct
from esphome.components.font import Font
from esphome.components.image import Image_
from esphome.components.sensor import Sensor
from esphome.components.text_sensor import TextSensor
import esphome.config_validation as cv
from esphome.const import CONF_ARGS, CONF_COLOR, CONF_FORMAT, CONF_VALUE
from esphome.core import HexInt
from esphome.helpers import cpp_string_escape
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor

from . import defines as df, types as ty
from .defines import LV_FONTS, LvConstant
from .helpers import esphome_fonts_used, lv_fonts_used, lvgl_components_required
from .types import LValidator

# List of other components used


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
    lvgl_components_required.add(CONF_COLOR)
    return f"lv_color_from({value})"


def lv_builtin_font(value):
    fontval = cv.one_of(*LV_FONTS, lower=True)(value)
    lv_fonts_used.add(fontval)
    return "&lv_font_" + fontval


def font(value):
    """Accept either the name of a built-in LVGL font, or the ID of an ESPHome font"""
    if value == SCHEMA_EXTRACT:
        return LV_FONTS
    if isinstance(value, str) and value.lower() in LV_FONTS:
        return lv_builtin_font(value)
    fontval = cv.use_id(Font)(value)
    esphome_fonts_used.add(fontval)
    return requires_component("font")(f"{fontval}_as_lv_font_")


def is_esphome_font(fontval):
    return "_as_lv_font_" in fontval


@schema_extractor("one_of")
def bool_(value):
    if value == SCHEMA_EXTRACT:
        return ["true", "false"]
    return "true" if cv.boolean(value) else "false"


def animated(value):
    if isinstance(value, bool):
        value = "ON" if value else "OFF"
    return LvConstant("LV_ANIM_", "OFF", "ON").one_of(value)


def key_code(value):
    value = cv.Any(cv.All(cv.string_strict, cv.Length(min=1, max=1)), cv.uint8_t)(value)
    if isinstance(value, str):
        return ord(value[0])
    return value


def id_name(value):
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


def zoom(value):
    value = cv.float_range(0.1, 10.0)(value)
    return int(value * 256)


def angle(value):
    """
    Validation for an angle in degrees, converted to an integer representing 0.1deg units
    :param value: The input in the range 0..360
    :return: An angle in 1/10 degree units.
    """
    return int(cv.float_range(0.0, 360.0)(cv.angle(value)) * 10)


def pixels(value):
    """A size in one axis - one of "size_content", a number (pixels) or a percentage"""
    if isinstance(value, str) and value.lower().endswith("px"):
        return cv.int_(value[:-2])
    return str(cv.int_(value))


@schema_extractor("one_of")
def size(value):
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
    consts = LvConstant("LV_OPA_", "TRANSP", "COVER")
    if value == SCHEMA_EXTRACT:
        return consts.choices
    value = cv.Any(cv.percentage, consts.one_of)(value)
    if isinstance(value, float):
        return int(value * 255)
    return value


def stop_value(value):
    return cv.int_range(0, 255)(value)


def optional_boolean(value):
    if value is None:
        return True
    return cv.boolean(value)


def point_list(il):
    il = cv.string(il)
    nl = il.replace(" ", "").split(",")
    return list(map(int, nl))


def option_string(value):
    value = cv.string(value).strip()
    if value.find("\n") != -1:
        raise cv.Invalid("Options strings must not contain newlines")
    return value


lv_color = LValidator(color, ty.lv_color_t, retmapper=color_retmapper)
lv_bool = LValidator(bool_, cg.bool_, BinarySensor, "get_state()")
lv_image = LValidator(
    cv.All(cv.use_id(Image_), requires_component("image")),
    ty.lv_img_t,
    retmapper=lambda x: f"lv_img_from({x})",
)


def lvms_validator_(value):
    if value == "never":
        value = "2147483647ms"
    return cv.positive_time_period_milliseconds(value)


lv_milliseconds = LValidator(
    lvms_validator_,
    cg.int32,
    retmapper=lambda x: x.total_milliseconds,
)


class TextValidator(LValidator):
    def __init__(self):
        super().__init__(
            cv.string,
            cg.const_char_ptr,
            TextSensor,
            "get_state().c_str()",
            lambda s: cg.safe_exp(f"{s}"),
        )

    def __call__(self, value):
        if isinstance(value, dict):
            return value
        return super().__call__(value)

    async def process(self, value, args=()):
        if isinstance(value, dict):
            args = [str(x) for x in value[CONF_ARGS]]
            arg_expr = cg.RawExpression(",".join(args))
            format_str = cpp_string_escape(value[CONF_FORMAT])
            return f"str_sprintf({format_str}, {arg_expr}).c_str()"
        return await super().process(value, args)


lv_text = TextValidator()
lv_float = LValidator(cv.float_, cg.float_, Sensor, "get_state()")
lv_int = LValidator(cv.int_, cg.int_, Sensor, "get_state()")
lv_brightness = LValidator(
    cv.percentage, cg.float_, Sensor, "get_state()", retmapper=lambda x: int(x * 255)
)


def lv_repeat_count(value):
    if isinstance(value, str) and value.lower() in ("forever", "infinite"):
        value = 0xFFFF
    return cv.positive_int(value)


async def get_end_value(config):
    return await lv_int.process(config.get(df.CONF_END_VALUE))


async def get_start_value(config):
    if df.CONF_START_VALUE in config:
        value = config[df.CONF_START_VALUE]
    else:
        value = config.get(CONF_VALUE)
    return await lv_int.process(value)
