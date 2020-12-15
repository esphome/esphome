import functools

from esphome import core
from esphome.components import display
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_FILE, CONF_GLYPHS, CONF_ID, CONF_SIZE
from esphome.core import CORE, HexInt

DEPENDENCIES = ['display']
MULTI_CONF = True

Font = display.display_ns.class_('Font')
Glyph = display.display_ns.class_('Glyph')


def validate_glyphs(value):
    if isinstance(value, list):
        value = cv.Schema([cv.string])(value)
    value = cv.Schema([cv.string])(list(value))

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
        raise cv.Invalid(f"Found duplicate glyph {x}")

    value.sort(key=functools.cmp_to_key(comparator))
    return value


def validate_pillow_installed(value):
    try:
        import PIL
    except ImportError as err:
        raise cv.Invalid("Please install the pillow python package to use this feature. "
                         "(pip install pillow)") from err

    if PIL.__version__[0] < '4':
        raise cv.Invalid("Please update your pillow installation to at least 4.0.x. "
                         "(pip install -U pillow)")

    return value


def validate_truetype_file(value):
    if value.endswith('.zip'):  # for Google Fonts downloads
        raise cv.Invalid("Please unzip the font archive '{}' first and then use the .ttf files "
                         "inside.".format(value))
    if not value.endswith('.ttf'):
        raise cv.Invalid("Only truetype (.ttf) files are supported. Please make sure you're "
                         "using the correct format or rename the extension to .ttf")
    return cv.file_(value)


DEFAULT_GLYPHS = ' !"%()+,-.:0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz°'
CONF_RAW_DATA_ID = 'raw_data_id'

FONT_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(Font),
    cv.Required(CONF_FILE): validate_truetype_file,
    cv.Optional(CONF_GLYPHS, default=DEFAULT_GLYPHS): validate_glyphs,
    cv.Optional(CONF_SIZE, default=20): cv.int_range(min=1),
    cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
})

CONFIG_SCHEMA = cv.All(validate_pillow_installed, FONT_SCHEMA)


def to_code(config):
    from PIL import ImageFont

    path = CORE.relative_config_path(config[CONF_FILE])
    try:
        font = ImageFont.truetype(path, config[CONF_SIZE])
    except Exception as e:
        raise core.EsphomeError(f"Could not load truetype file {path}: {e}")

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

    rhs = [HexInt(x) for x in data]
    prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)

    glyphs = []
    for glyph in config[CONF_GLYPHS]:
        glyphs.append(Glyph(glyph, prog_arr, *glyph_args[glyph]))

    cg.new_Pvariable(config[CONF_ID], glyphs, ascent, ascent + descent)
