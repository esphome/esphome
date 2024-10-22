from typing import Union

import esphome.codegen as cg
from esphome.components.color import CONF_HEX, ColorStruct, from_rgbw
from esphome.components.font import Font
from esphome.components.image import Image_
import esphome.config_validation as cv
from esphome.const import (
    CONF_ARGS,
    CONF_COLOR,
    CONF_FORMAT,
    CONF_ID,
    CONF_TIME,
    CONF_VALUE,
)
from esphome.core import CORE, ID, Lambda
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
from .helpers import esphome_fonts_used, lv_fonts_used, requires_component
from .types import lv_font_t, lv_gradient_t, lv_img_t

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

COLOR_NAMES = {
    "aliceblue": 0xF0F8FF,
    "antiquewhite": 0xFAEBD7,
    "aqua": 0x00FFFF,
    "aquamarine": 0x7FFFD4,
    "azure": 0xF0FFFF,
    "beige": 0xF5F5DC,
    "bisque": 0xFFE4C4,
    "black": 0x000000,
    "blanchedalmond": 0xFFEBCD,
    "blue": 0x0000FF,
    "blueviolet": 0x8A2BE2,
    "brown": 0xA52A2A,
    "burlywood": 0xDEB887,
    "cadetblue": 0x5F9EA0,
    "chartreuse": 0x7FFF00,
    "chocolate": 0xD2691E,
    "coral": 0xFF7F50,
    "cornflowerblue": 0x6495ED,
    "cornsilk": 0xFFF8DC,
    "crimson": 0xDC143C,
    "cyan": 0x00FFFF,
    "darkblue": 0x00008B,
    "darkcyan": 0x008B8B,
    "darkgoldenrod": 0xB8860B,
    "darkgray": 0xA9A9A9,
    "darkgreen": 0x006400,
    "darkgrey": 0xA9A9A9,
    "darkkhaki": 0xBDB76B,
    "darkmagenta": 0x8B008B,
    "darkolivegreen": 0x556B2F,
    "darkorange": 0xFF8C00,
    "darkorchid": 0x9932CC,
    "darkred": 0x8B0000,
    "darksalmon": 0xE9967A,
    "darkseagreen": 0x8FBC8F,
    "darkslateblue": 0x483D8B,
    "darkslategray": 0x2F4F4F,
    "darkslategrey": 0x2F4F4F,
    "darkturquoise": 0x00CED1,
    "darkviolet": 0x9400D3,
    "deeppink": 0xFF1493,
    "deepskyblue": 0x00BFFF,
    "dimgray": 0x696969,
    "dimgrey": 0x696969,
    "dodgerblue": 0x1E90FF,
    "firebrick": 0xB22222,
    "floralwhite": 0xFFFAF0,
    "forestgreen": 0x228B22,
    "fuchsia": 0xFF00FF,
    "gainsboro": 0xDCDCDC,
    "ghostwhite": 0xF8F8FF,
    "goldenrod": 0xDAA520,
    "gold": 0xFFD700,
    "gray": 0x808080,
    "green": 0x008000,
    "greenyellow": 0xADFF2F,
    "grey": 0x808080,
    "honeydew": 0xF0FFF0,
    "hotpink": 0xFF69B4,
    "indianred": 0xCD5C5C,
    "indigo": 0x4B0082,
    "ivory": 0xFFFFF0,
    "khaki": 0xF0E68C,
    "lavenderblush": 0xFFF0F5,
    "lavender": 0xE6E6FA,
    "lawngreen": 0x7CFC00,
    "lemonchiffon": 0xFFFACD,
    "lightblue": 0xADD8E6,
    "lightcoral": 0xF08080,
    "lightcyan": 0xE0FFFF,
    "lightgoldenrodyellow": 0xFAFAD2,
    "lightgray": 0xD3D3D3,
    "lightgreen": 0x90EE90,
    "lightgrey": 0xD3D3D3,
    "lightpink": 0xFFB6C1,
    "lightsalmon": 0xFFA07A,
    "lightseagreen": 0x20B2AA,
    "lightskyblue": 0x87CEFA,
    "lightslategray": 0x778899,
    "lightslategrey": 0x778899,
    "lightsteelblue": 0xB0C4DE,
    "lightyellow": 0xFFFFE0,
    "lime": 0x00FF00,
    "limegreen": 0x32CD32,
    "linen": 0xFAF0E6,
    "magenta": 0xFF00FF,
    "maroon": 0x800000,
    "mediumaquamarine": 0x66CDAA,
    "mediumblue": 0x0000CD,
    "mediumorchid": 0xBA55D3,
    "mediumpurple": 0x9370DB,
    "mediumseagreen": 0x3CB371,
    "mediumslateblue": 0x7B68EE,
    "mediumspringgreen": 0x00FA9A,
    "mediumturquoise": 0x48D1CC,
    "mediumvioletred": 0xC71585,
    "midnightblue": 0x191970,
    "mintcream": 0xF5FFFA,
    "mistyrose": 0xFFE4E1,
    "moccasin": 0xFFE4B5,
    "navajowhite": 0xFFDEAD,
    "navy": 0x000080,
    "oldlace": 0xFDF5E6,
    "olive": 0x808000,
    "olivedrab": 0x6B8E23,
    "orange": 0xFFA500,
    "orangered": 0xFF4500,
    "orchid": 0xDA70D6,
    "palegoldenrod": 0xEEE8AA,
    "palegreen": 0x98FB98,
    "paleturquoise": 0xAFEEEE,
    "palevioletred": 0xDB7093,
    "papayawhip": 0xFFEFD5,
    "peachpuff": 0xFFDAB9,
    "peru": 0xCD853F,
    "pink": 0xFFC0CB,
    "plum": 0xDDA0DD,
    "powderblue": 0xB0E0E6,
    "purple": 0x800080,
    "rebeccapurple": 0x663399,
    "red": 0xFF0000,
    "rosybrown": 0xBC8F8F,
    "royalblue": 0x4169E1,
    "saddlebrown": 0x8B4513,
    "salmon": 0xFA8072,
    "sandybrown": 0xF4A460,
    "seagreen": 0x2E8B57,
    "seashell": 0xFFF5EE,
    "sienna": 0xA0522D,
    "silver": 0xC0C0C0,
    "skyblue": 0x87CEEB,
    "slateblue": 0x6A5ACD,
    "slategray": 0x708090,
    "slategrey": 0x708090,
    "snow": 0xFFFAFA,
    "springgreen": 0x00FF7F,
    "steelblue": 0x4682B4,
    "tan": 0xD2B48C,
    "teal": 0x008080,
    "thistle": 0xD8BFD8,
    "tomato": 0xFF6347,
    "turquoise": 0x40E0D0,
    "violet": 0xEE82EE,
    "wheat": 0xF5DEB3,
    "white": 0xFFFFFF,
    "whitesmoke": 0xF5F5F5,
    "yellow": 0xFFFF00,
    "yellowgreen": 0x9ACD32,
}


@schema_extractor("one_of")
def color(value):
    if value == SCHEMA_EXTRACT:
        return ["hex color value", "color ID"]
    return cv.Any(cv.int_, cv.one_of(*COLOR_NAMES, lower=True), cv.use_id(ColorStruct))(
        value
    )


def color_retmapper(value):
    if isinstance(value, cv.Lambda):
        return cv.returning_lambda(value)
    if isinstance(value, str) and value in COLOR_NAMES:
        value = COLOR_NAMES[value]
    if isinstance(value, int):
        return literal(
            f"lv_color_make({(value >> 16) & 0xFF}, {(value >> 8) & 0xFF}, {value & 0xFF})"
        )
    if isinstance(value, ID):
        cval = [x for x in CORE.config[CONF_COLOR] if x[CONF_ID] == value][0]
        if CONF_HEX in cval:
            r, g, b = cval[CONF_HEX]
        else:
            r, g, b, _ = from_rgbw(cval)
        return literal(f"lv_color_make({r}, {g}, {b})")
    assert False


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
    if isinstance(value, str) and value.lower().endswith("px"):
        value = cv.int_(value[:-2])
    value = cv.Any(cv.int_, cv.percentage)(value)
    if isinstance(value, int):
        return value
    return f"lv_pct({int(value * 100)})"


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


lv_angle = LValidator(angle, uint32)


@schema_extractor("one_of")
def size_validator(value):
    """A size in one axis - one of "size_content", a number (pixels) or a percentage"""
    if value == SCHEMA_EXTRACT:
        return ["SIZE_CONTENT", "number of pixels", "percentage"]
    if isinstance(value, str) and value.lower().endswith("px"):
        value = cv.int_(value[:-2])
    if isinstance(value, str) and value.upper() == "SIZE_CONTENT":
        return "LV_SIZE_CONTENT"
    return pixels_or_percent_validator(value)


size = LValidator(size_validator, uint32, retmapper=literal)


def pixels_validator(value):
    if isinstance(value, str) and value.lower().endswith("px"):
        return cv.int_(value[:-2])
    return cv.int_(value)


pixels = LValidator(pixels_validator, uint32, retmapper=literal)

radius_consts = LvConstant("LV_RADIUS_", "CIRCLE")


@schema_extractor("one_of")
def fraction_validator(value):
    if value == SCHEMA_EXTRACT:
        return radius_consts.choices
    value = cv.Any(size, cv.percentage, radius_consts.one_of)(value)
    if isinstance(value, float):
        return int(value * 255)
    return value


lv_fraction = LValidator(fraction_validator, uint32, retmapper=literal)


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
    retmapper=lambda x: MockObj(x, "->").get_lv_img_dsc(),
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
lv_positive_int = LValidator(cv.positive_int, cg.int_)
lv_brightness = LValidator(cv.percentage, cg.float_, retmapper=lambda x: int(x * 255))


def gradient_mapper(value):
    return MockObj(value)


def gradient_validator(value):
    return cv.use_id(lv_gradient_t)(value)


lv_gradient = LValidator(
    validator=gradient_validator,
    rtype=lv_gradient_t,
    retmapper=gradient_mapper,
)


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
