# coding=utf-8
import voluptuous as vol

from esphome import core
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import CONF_FILE, CONF_GLYPHS, CONF_ID, CONF_SIZE
from esphome.core import CORE, HexInt
from esphome.cpp_generator import Pvariable, progmem_array, safe_exp
from esphome.cpp_types import App, uint8
from esphome.py_compat import sort_by_cmp

DEPENDENCIES = ['display']
MULTI_CONF = True

Font = display.display_ns.class_('Font')
Glyph = display.display_ns.class_('Glyph')


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
        if len(x_) > len(y_):
            return 1
        raise vol.Invalid(u"Found duplicate glyph {}".format(x))

    sort_by_cmp(value, comparator)
    return value


def validate_pillow_installed(value):
    try:
        import PIL
    except ImportError:
        raise vol.Invalid("Please install the pillow python package to use this feature. "
                          "(pip install pillow)")

    if PIL.__version__[0] < '4':
        raise vol.Invalid("Please update your pillow installation to at least 4.0.x. "
                          "(pip install -U pillow)")

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
    cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_variable_id(uint8),
})

CONFIG_SCHEMA = vol.All(validate_pillow_installed, FONT_SCHEMA)


def to_code(config):
    from PIL import ImageFont

    path = CORE.relative_path(config[CONF_FILE])
    try:
        font = ImageFont.truetype(path, config[CONF_SIZE])
    except Exception as e:
        raise core.EsphomeError(u"Could not load truetype file {}: {}".format(path, e))

    ascent, descent = font.getmetrics()

    glyph_args = {}
    data = []
    for glyph in config[CONF_GLYPHS]:
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

    rhs = safe_exp([HexInt(x) for x in data])
    prog_arr = progmem_array(config[CONF_RAW_DATA_ID], rhs)

    glyphs = []
    for glyph in config[CONF_GLYPHS]:
        glyphs.append(Glyph(glyph, prog_arr, *glyph_args[glyph]))

    rhs = App.make_font(glyphs, ascent, ascent + descent)
    Pvariable(config[CONF_ID], rhs)
