# coding=utf-8
import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import core
from esphomeyaml.components import display
from esphomeyaml.const import CONF_FILE, CONF_GLYPHS, CONF_ID, CONF_SIZE
from esphomeyaml.core import HexInt
from esphomeyaml.helpers import App, ArrayInitializer, MockObj, Pvariable, RawExpression, add, \
    relative_path

DEPENDENCIES = ['display']

Font = display.display_ns.Font
Glyph = display.display_ns.Glyph


def validate_glyphs(value):
    if isinstance(value, list):
        value = vol.Schema([cv.string])(value)
    value = vol.Schema([cv.string])(list(value))

    def comparator(x, y):
        x_ = x.encode('utf-8')
        y_ = y.encode('utf-8')

        for c in range(min(len(x_), len(y_))):
            if x_[c] < y_[c]:
                return -1
            if x_[c] > y_[c]:
                return 1

        if len(x_) < len(y_):
            return -1
        elif len(x_) > len(y_):
            return 1
        else:
            raise vol.Invalid(u"Found duplicate glyph {}".format(x))

    value.sort(cmp=comparator)
    return value


def validate_pillow_installed(value):
    try:
        import PIL
    except ImportError:
        raise vol.Invalid("Please install the pillow python package to use this feature. "
                          "(pip2 install pillow)")

    if PIL.__version__[0] < '4':
        raise vol.Invalid("Please update your pillow installation to at least 4.0.x. "
                          "(pip2 install -U pillow)")

    return value


def validate_truetype_file(value):
    if value.endswith('.zip'):  # for Google Fonts downloads
        raise vol.Invalid(u"Please unzip the font archive '{}' first and then use the .ttf files "
                          u"inside.".format(value))
    if not value.endswith('.ttf'):
        raise vol.Invalid(u"Only truetype (.ttf) files are supported. Please make sure you're "
                          u"using the correct format or rename the extension to .ttf")
    return cv.file_(value)


DEFAULT_GLYPHS = u' !"%()+,-.:0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz°'
CONF_RAW_DATA_ID = 'raw_data_id'

FONT_SCHEMA = vol.Schema({
    vol.Required(CONF_ID): cv.declare_variable_id(Font),
    vol.Required(CONF_FILE): validate_truetype_file,
    vol.Optional(CONF_GLYPHS, default=DEFAULT_GLYPHS): validate_glyphs,
    vol.Optional(CONF_SIZE, default=20): vol.All(cv.int_, vol.Range(min=1)),
    cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_variable_id(None),
})

CONFIG_SCHEMA = vol.All(validate_pillow_installed, cv.ensure_list, [FONT_SCHEMA])


def to_code(config):
    from PIL import ImageFont

    for conf in config:
        path = relative_path(conf[CONF_FILE])
        try:
            font = ImageFont.truetype(path, conf[CONF_SIZE])
        except Exception as e:
            raise core.ESPHomeYAMLError(u"Could not load truetype file {}: {}".format(path, e))

        ascent, descent = font.getmetrics()

        glyph_args = {}
        data = []
        for glyph in conf[CONF_GLYPHS]:
            mask = font.getmask(glyph, mode='1')
            _, (offset_x, offset_y) = font.font.getsize(glyph)
            width, height = mask.size
            width8 = ((width + 7) // 8) * 8
            glyph_data = [0 for _ in range(height * width8 // 8)]  # noqa: F812
            for y in range(height):
                for x in range(width):
                    if not mask.getpixel((x, y)):
                        continue
                    pos = x + y * width8
                    glyph_data[pos // 8] |= 0x80 >> (pos % 8)
            glyph_args[glyph] = (len(data), offset_x, offset_y, width, height)
            data += glyph_data

        raw_data = MockObj(conf[CONF_RAW_DATA_ID])
        add(RawExpression('static const uint8_t {}[{}] PROGMEM = {}'.format(
            raw_data, len(data),
            ArrayInitializer(*[HexInt(x) for x in data], multiline=False))))

        glyphs = []
        for glyph in conf[CONF_GLYPHS]:
            glyphs.append(Glyph(glyph, raw_data, *glyph_args[glyph]))

        rhs = App.make_font(ArrayInitializer(*glyphs), ascent, ascent + descent)
        Pvariable(conf[CONF_ID], rhs)
