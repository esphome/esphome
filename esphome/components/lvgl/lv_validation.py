import esphome.codegen as cg
from esphome.components.binary_sensor import BinarySensor
from esphome.components.color import ColorStruct
from esphome.components.font import Font
from esphome.components.sensor import Sensor
from esphome.components.text_sensor import TextSensor
import esphome.config_validation as cv
from esphome.const import CONF_ARGS, CONF_COLOR, CONF_FORMAT
from esphome.core import HexInt
from esphome.cpp_generator import MockObj
from esphome.helpers import cpp_string_escape
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor

from . import types as ty
from .defines import LV_FONTS, LValidator, LvConstant
from .helpers import (
    esphome_fonts_used,
    lv_fonts_used,
    lvgl_components_required,
    requires_component,
)
from .lvcode import ConstantLiteral, lv_expr
from .types import lv_font_t


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
        return lv_expr.color_hex(hexval)
    # Must be an id
    lvgl_components_required.add(CONF_COLOR)
    return lv_expr.color_from(MockObj(value))


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


lv_color = LValidator(color, ty.lv_color_t, retmapper=color_retmapper)
lv_bool = LValidator(cv.boolean, cg.bool_, BinarySensor, "get_state()")


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


class LvFont(LValidator):
    def __init__(self):
        def lv_builtin_font(value):
            fontval = cv.one_of(*LV_FONTS, lower=True)(value)
            lv_fonts_used.add(fontval)
            return "&lv_font_" + fontval

        def validator(value):
            if value == SCHEMA_EXTRACT:
                return LV_FONTS
            if isinstance(value, str) and value.lower() in LV_FONTS:
                return lv_builtin_font(value)
            fontval = cv.use_id(Font)(value)
            esphome_fonts_used.add(fontval)
            return requires_component("font")(f"{fontval}_engine->get_lv_font()")

        super().__init__(validator, lv_font_t)

    async def process(self, value, args=()):
        return ConstantLiteral(value)


lv_font = LvFont()
