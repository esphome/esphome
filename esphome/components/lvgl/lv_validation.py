from typing import Union

import esphome.codegen as cg
from esphome.components.color import ColorStruct
from esphome.components.font import Font
from esphome.components.image import Image_
import esphome.config_validation as cv
from esphome.const import CONF_ARGS, CONF_COLOR, CONF_FORMAT, CONF_TIME, CONF_VALUE
from esphome.core import HexInt, Lambda
from esphome.cpp_generator import MockObj
from esphome.cpp_types import ESPTime, uint32
from esphome.helpers import cpp_string_escape
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor

from . import types as ty
from .defines import (
    CONF_END_VALUE,
    CONF_START_VALUE,
    CONF_TIME_FORMAT,
    LV_FONTS,
    LValidator,
    LvConstant,
    call_lambda,
    literal,
)
from .helpers import (
    esphome_fonts_used,
    lv_fonts_used,
    lvgl_components_required,
    requires_component,
)
from .lvcode import lv_expr
from .types import lv_font_t, lv_img_t

opacity_consts = LvConstant("LV_OPA_", "TRANSP", "COVER")


@schema_extractor("one_of")
def opacity_validator(value):
    if value == SCHEMA_EXTRACT:
        return opacity_consts.choices
    value = cv.Any(cv.percentage, opacity_consts.one_of)(value)
    if isinstance(value, float):
        return int(value * 255)
    return value


opacity = LValidator(opacity_validator, uint32, retmapper=literal)


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


def option_string(value):
    value = cv.string(value).strip()
    if value.find("\n") != -1:
        raise cv.Invalid("Options strings must not contain newlines")
    return value


lv_color = LValidator(color, ty.lv_color_t, retmapper=color_retmapper)


def pixels_or_percent_validator(value):
    """A length in one axis - either a number (pixels) or a percentage"""
    if value == SCHEMA_EXTRACT:
        return ["pixels", "..%"]
    if isinstance(value, int):
        return cv.int_(value)
    # Will throw an exception if not a percentage.
    return f"lv_pct({int(cv.percentage(value) * 100)})"


pixels_or_percent = LValidator(pixels_or_percent_validator, uint32, retmapper=literal)


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
def size_validator(value):
    """A size in one axis - one of "size_content", a number (pixels) or a percentage"""
    if value == SCHEMA_EXTRACT:
        return ["SIZE_CONTENT", "number of pixels", "percentage"]
    if isinstance(value, str) and value.lower().endswith("px"):
        value = cv.int_(value[:-2])
    if isinstance(value, str) and not value.endswith("%"):
        if value.upper() == "SIZE_CONTENT":
            return "LV_SIZE_CONTENT"
        raise cv.Invalid("must be 'size_content', a percentage or an integer (pixels)")
    if isinstance(value, int):
        return cv.int_(value)
    # Will throw an exception if not a percentage.
    return f"lv_pct({int(cv.percentage(value) * 100)})"


size = LValidator(size_validator, uint32, retmapper=literal)


def pixels_validator(value):
    if isinstance(value, str) and value.lower().endswith("px"):
        return cv.int_(value[:-2])
    return cv.int_(value)


pixels = LValidator(pixels_validator, uint32, retmapper=literal)

radius_consts = LvConstant("LV_RADIUS_", "CIRCLE")


@schema_extractor("one_of")
def radius_validator(value):
    if value == SCHEMA_EXTRACT:
        return radius_consts.choices
    value = cv.Any(size, cv.percentage, radius_consts.one_of)(value)
    if isinstance(value, float):
        return int(value * 255)
    return value


radius = LValidator(radius_validator, uint32, retmapper=literal)


def id_name(value):
    if value == SCHEMA_EXTRACT:
        return "id"
    return cv.validate_id_name(value)


def stop_value(value):
    return cv.int_range(0, 255)(value)


lv_images_used = set()


def image_validator(value):
    value = requires_component("image")(value)
    value = cv.use_id(Image_)(value)
    lv_images_used.add(value)
    return value


lv_image = LValidator(
    image_validator,
    lv_img_t,
    retmapper=lambda x: lv_expr.img_from(MockObj(x)),
    requires="image",
)
lv_bool = LValidator(cv.boolean, cg.bool_, retmapper=literal)


def lv_pct(value: Union[int, float]):
    if isinstance(value, float):
        value = int(value * 100)
    return literal(f"lv_pct({value})")


def lvms_validator_(value):
    if value == "never":
        value = "2147483647ms"
    return cv.positive_time_period_milliseconds(value)


lv_milliseconds = LValidator(
    lvms_validator_, cg.int32, retmapper=lambda x: x.total_milliseconds
)


class TextValidator(LValidator):
    def __init__(self):
        super().__init__(cv.string, cg.std_string, lambda s: cg.safe_exp(f"{s}"))

    def __call__(self, value):
        if isinstance(value, dict) and CONF_FORMAT in value:
            return value
        return super().__call__(value)

    async def process(self, value, args=()):
        if isinstance(value, dict):
            if format_str := value.get(CONF_FORMAT):
                args = [str(x) for x in value[CONF_ARGS]]
                arg_expr = cg.RawExpression(",".join(args))
                format_str = cpp_string_escape(format_str)
                return literal(f"str_sprintf({format_str}, {arg_expr}).c_str()")
            if time_format := value.get(CONF_TIME_FORMAT):
                source = value[CONF_TIME]
                if isinstance(source, Lambda):
                    time_format = cpp_string_escape(time_format)
                    return cg.RawExpression(
                        call_lambda(
                            await cg.process_lambda(source, args, return_type=ESPTime)
                        )
                        + f".strftime({time_format}).c_str()"
                    )
                # must be an ID
                source = await cg.get_variable(source)
                return source.now().strftime(time_format).c_str()
        if isinstance(value, Lambda):
            value = call_lambda(
                await cg.process_lambda(value, args, return_type=self.rtype)
            )

            # Was the lambda call reduced to a string?
            if value.endswith("c_str()") or (
                value.endswith('"') and value.startswith('"')
            ):
                pass
            else:
                # Either a std::string or a lambda call returning that. We need const char*
                value = f"({value}).c_str()"
            return cg.RawExpression(value)
        return await super().process(value, args)


lv_text = TextValidator()
lv_float = LValidator(cv.float_, cg.float_)
lv_int = LValidator(cv.int_, cg.int_)
lv_brightness = LValidator(cv.percentage, cg.float_, retmapper=lambda x: int(x * 255))


def is_lv_font(font):
    return isinstance(font, str) and font.lower() in LV_FONTS


class LvFont(LValidator):
    def __init__(self):
        def lv_builtin_font(value):
            fontval = cv.one_of(*LV_FONTS, lower=True)(value)
            lv_fonts_used.add(fontval)
            return fontval

        def validator(value):
            if value == SCHEMA_EXTRACT:
                return LV_FONTS
            if is_lv_font(value):
                return lv_builtin_font(value)
            fontval = cv.use_id(Font)(value)
            esphome_fonts_used.add(fontval)
            return requires_component("font")(fontval)

        super().__init__(validator, lv_font_t)

    async def process(self, value, args=()):
        if is_lv_font(value):
            return literal(f"&lv_font_{value}")
        return literal(f"{value}_engine->get_lv_font()")


lv_font = LvFont()


def animated(value):
    if isinstance(value, bool):
        value = "ON" if value else "OFF"
    return LvConstant("LV_ANIM_", "OFF", "ON").one_of(value)


def key_code(value):
    value = cv.Any(cv.All(cv.string_strict, cv.Length(min=1, max=1)), cv.uint8_t)(value)
    if isinstance(value, str):
        return ord(value[0])
    return value


async def get_end_value(config):
    return await lv_int.process(config.get(CONF_END_VALUE))


async def get_start_value(config):
    if CONF_START_VALUE in config:
        value = config[CONF_START_VALUE]
    else:
        value = config.get(CONF_VALUE)
    return await lv_int.process(value)
