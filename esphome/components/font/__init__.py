import functools
import hashlib
import logging
import os
from pathlib import Path
import re

from packaging import version
import requests

from esphome import core, external_files
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_FAMILY,
    CONF_FILE,
    CONF_GLYPHS,
    CONF_ID,
    CONF_PATH,
    CONF_RAW_DATA_ID,
    CONF_REFRESH,
    CONF_SIZE,
    CONF_TYPE,
    CONF_URL,
    CONF_WEIGHT,
)
from esphome.core import CORE, HexInt
from esphome.helpers import copy_file_if_changed, cpp_string_escape

_LOGGER = logging.getLogger(__name__)

DOMAIN = "font"
MULTI_CONF = True

CODEOWNERS = ["@esphome/core", "@clydebarrow"]

font_ns = cg.esphome_ns.namespace("font")

Font = font_ns.class_("Font")
Glyph = font_ns.class_("Glyph")
GlyphData = font_ns.struct("GlyphData")

CONF_BPP = "bpp"
CONF_EXTRAS = "extras"
CONF_FONTS = "fonts"


def glyph_comparator(x, y):
    x_ = x.encode("utf-8")
    y_ = y.encode("utf-8")

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


def validate_glyphs(value):
    if isinstance(value, list):
        value = cv.Schema([cv.string])(value)
    value = cv.Schema([cv.string])(list(value))

    value.sort(key=functools.cmp_to_key(glyph_comparator))
    return value


font_map = {}


def merge_glyphs(config):
    glyphs = []
    glyphs.extend(config[CONF_GLYPHS])
    font_list = [(EFont(config[CONF_FILE], config[CONF_SIZE], config[CONF_GLYPHS]))]
    if extras := config.get(CONF_EXTRAS):
        extra_fonts = list(
            map(
                lambda x: EFont(x[CONF_FILE], config[CONF_SIZE], x[CONF_GLYPHS]), extras
            )
        )
        font_list.extend(extra_fonts)
        for extra in extras:
            glyphs.extend(extra[CONF_GLYPHS])
        validate_glyphs(glyphs)
    font_map[config[CONF_ID]] = font_list
    return config


def validate_pillow_installed(value):
    try:
        import PIL
    except ImportError as err:
        raise cv.Invalid(
            "Please install the pillow python package to use this feature. "
            '(pip install "pillow==10.2.0")'
        ) from err

    if version.parse(PIL.__version__) != version.parse("10.2.0"):
        raise cv.Invalid(
            "Please update your pillow installation to 10.2.0. "
            '(pip install "pillow==10.2.0")'
        )

    return value


FONT_EXTENSIONS = (".ttf", ".woff", ".otf")


def validate_truetype_file(value):
    if value.lower().endswith(".zip"):  # for Google Fonts downloads
        raise cv.Invalid(
            f"Please unzip the font archive '{value}' first and then use the .ttf files inside."
        )
    if not any(map(value.lower().endswith, FONT_EXTENSIONS)):
        raise cv.Invalid(f"Only {FONT_EXTENSIONS} files are supported.")
    return cv.file_(value)


TYPE_LOCAL = "local"
TYPE_LOCAL_BITMAP = "local_bitmap"
TYPE_GFONTS = "gfonts"
TYPE_WEB = "web"
LOCAL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PATH): validate_truetype_file,
    }
)

LOCAL_BITMAP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PATH): cv.file_,
    }
)

CONF_ITALIC = "italic"
FONT_WEIGHTS = {
    "thin": 100,
    "extra-light": 200,
    "light": 300,
    "regular": 400,
    "medium": 500,
    "semi-bold": 600,
    "bold": 700,
    "extra-bold": 800,
    "black": 900,
}


def validate_weight_name(value):
    return FONT_WEIGHTS[cv.one_of(*FONT_WEIGHTS, lower=True, space="-")(value)]


def _compute_local_font_path(value: dict) -> Path:
    url = value[CONF_URL]
    h = hashlib.new("sha256")
    h.update(url.encode())
    key = h.hexdigest()[:8]
    base_dir = external_files.compute_local_file_dir(DOMAIN)
    _LOGGER.debug("_compute_local_font_path: base_dir=%s", base_dir / key)
    return base_dir / key


def get_font_path(value, type) -> Path:
    if type == TYPE_GFONTS:
        name = f"{value[CONF_FAMILY]}@{value[CONF_WEIGHT]}@{value[CONF_ITALIC]}@v1"
        return external_files.compute_local_file_dir(DOMAIN) / f"{name}.ttf"
    if type == TYPE_WEB:
        return _compute_local_font_path(value) / "font.ttf"
    return None


def download_gfont(value):
    name = (
        f"{value[CONF_FAMILY]}:ital,wght@{int(value[CONF_ITALIC])},{value[CONF_WEIGHT]}"
    )
    url = f"https://fonts.googleapis.com/css2?family={name}"
    path = get_font_path(value, TYPE_GFONTS)
    _LOGGER.debug("download_gfont: path=%s", path)

    try:
        req = requests.get(url, timeout=external_files.NETWORK_TIMEOUT)
        req.raise_for_status()
    except requests.exceptions.RequestException as e:
        raise cv.Invalid(
            f"Could not download font at {url}, please check the fonts exists "
            f"at google fonts ({e})"
        )
    match = re.search(r"src:\s+url\((.+)\)\s+format\('truetype'\);", req.text)
    if match is None:
        raise cv.Invalid(
            f"Could not extract ttf file from gfonts response for {name}, "
            f"please report this."
        )

    ttf_url = match.group(1)
    _LOGGER.debug("download_gfont: ttf_url=%s", ttf_url)

    external_files.download_content(ttf_url, path)
    return value


def download_web_font(value):
    url = value[CONF_URL]
    path = get_font_path(value, TYPE_WEB)

    external_files.download_content(url, path)
    _LOGGER.debug("download_web_font: path=%s", path)
    return value


EXTERNAL_FONT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WEIGHT, default="regular"): cv.Any(
            cv.int_, validate_weight_name
        ),
        cv.Optional(CONF_ITALIC, default=False): cv.boolean,
        cv.Optional(CONF_REFRESH, default="1d"): cv.All(cv.string, cv.source_refresh),
    }
)


GFONTS_SCHEMA = cv.All(
    EXTERNAL_FONT_SCHEMA.extend(
        {
            cv.Required(CONF_FAMILY): cv.string_strict,
        }
    ),
    download_gfont,
)

WEB_FONT_SCHEMA = cv.All(
    EXTERNAL_FONT_SCHEMA.extend(
        {
            cv.Required(CONF_URL): cv.string_strict,
        }
    ),
    download_web_font,
)


def validate_file_shorthand(value):
    value = cv.string_strict(value)
    if value.startswith("gfonts://"):
        match = re.match(r"^gfonts://([^@]+)(@.+)?$", value)
        if match is None:
            raise cv.Invalid("Could not parse gfonts shorthand syntax, please check it")
        family = match.group(1)
        weight = match.group(2)
        data = {
            CONF_TYPE: TYPE_GFONTS,
            CONF_FAMILY: family,
        }
        if weight is not None:
            data[CONF_WEIGHT] = weight[1:]
        return FILE_SCHEMA(data)

    if value.startswith("http://") or value.startswith("https://"):
        return FILE_SCHEMA(
            {
                CONF_TYPE: TYPE_WEB,
                CONF_URL: value,
            }
        )

    if value.endswith(".pcf") or value.endswith(".bdf"):
        return FILE_SCHEMA(
            {
                CONF_TYPE: TYPE_LOCAL_BITMAP,
                CONF_PATH: value,
            }
        )

    return FILE_SCHEMA(
        {
            CONF_TYPE: TYPE_LOCAL,
            CONF_PATH: value,
        }
    )


TYPED_FILE_SCHEMA = cv.typed_schema(
    {
        TYPE_LOCAL: LOCAL_SCHEMA,
        TYPE_GFONTS: GFONTS_SCHEMA,
        TYPE_LOCAL_BITMAP: LOCAL_BITMAP_SCHEMA,
        TYPE_WEB: WEB_FONT_SCHEMA,
    }
)


def _file_schema(value):
    if isinstance(value, str):
        return validate_file_shorthand(value)
    return TYPED_FILE_SCHEMA(value)


FILE_SCHEMA = cv.All(_file_schema)

DEFAULT_GLYPHS = (
    ' !"%()+=,-.:/?0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz°'
)
CONF_RAW_GLYPH_ID = "raw_glyph_id"

FONT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(Font),
        cv.Required(CONF_FILE): FILE_SCHEMA,
        cv.Optional(CONF_GLYPHS, default=DEFAULT_GLYPHS): validate_glyphs,
        cv.Optional(CONF_SIZE, default=20): cv.int_range(min=1),
        cv.Optional(CONF_BPP, default=1): cv.one_of(1, 2, 4, 8),
        cv.Optional(CONF_EXTRAS): cv.ensure_list(
            cv.Schema(
                {
                    cv.Required(CONF_FILE): FILE_SCHEMA,
                    cv.Required(CONF_GLYPHS): validate_glyphs,
                }
            )
        ),
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        cv.GenerateID(CONF_RAW_GLYPH_ID): cv.declare_id(GlyphData),
    },
)

CONFIG_SCHEMA = cv.All(validate_pillow_installed, FONT_SCHEMA, merge_glyphs)


# PIL doesn't provide a consistent interface for both TrueType and bitmap
# fonts. So, we use our own wrappers to give us the consistency that we need.


class TrueTypeFontWrapper:
    def __init__(self, font):
        self.font = font

    def getoffset(self, glyph):
        _, (offset_x, offset_y) = self.font.font.getsize(glyph)
        return offset_x, offset_y

    def getmask(self, glyph, **kwargs):
        return self.font.getmask(glyph, **kwargs)

    def getmetrics(self, glyphs):
        return self.font.getmetrics()


class BitmapFontWrapper:
    def __init__(self, font):
        self.font = font
        self.max_height = 0

    def getoffset(self, glyph):
        return 0, 0

    def getmask(self, glyph, **kwargs):
        return self.font.getmask(glyph, **kwargs)

    def getmetrics(self, glyphs):
        max_height = 0
        for glyph in glyphs:
            mask = self.getmask(glyph, mode="1")
            _, height = mask.size
            max_height = max(max_height, height)
        return (max_height, 0)


class EFont:
    def __init__(self, file, size, glyphs):
        self.glyphs = glyphs
        ftype = file[CONF_TYPE]
        if ftype == TYPE_LOCAL_BITMAP:
            font = load_bitmap_font(CORE.relative_config_path(file[CONF_PATH]))
        elif ftype == TYPE_LOCAL:
            path = CORE.relative_config_path(file[CONF_PATH])
            font = load_ttf_font(path, size)
        elif ftype in (TYPE_GFONTS, TYPE_WEB):
            path = get_font_path(file, ftype)
            font = load_ttf_font(path, size)
        else:
            raise cv.Invalid(f"Could not load font: unknown type: {ftype}")
        self.font = font
        self.ascent, self.descent = font.getmetrics(glyphs)

    def has_glyph(self, glyph):
        return glyph in self.glyphs


def convert_bitmap_to_pillow_font(filepath):
    from PIL import BdfFontFile, PcfFontFile

    local_bitmap_font_file = external_files.compute_local_file_dir(
        DOMAIN,
    ) / os.path.basename(filepath)

    copy_file_if_changed(filepath, local_bitmap_font_file)

    with open(local_bitmap_font_file, "rb") as fp:
        try:
            try:
                p = PcfFontFile.PcfFontFile(fp)
            except SyntaxError:
                fp.seek(0)
                p = BdfFontFile.BdfFontFile(fp)

            # Convert to pillow-formatted fonts, which have a .pil and .pbm extension.
            p.save(local_bitmap_font_file)
        except (SyntaxError, OSError) as err:
            raise core.EsphomeError(
                f"Failed to parse as bitmap font: '{filepath}': {err}"
            )

    local_pil_font_file = os.path.splitext(local_bitmap_font_file)[0] + ".pil"
    return cv.file_(local_pil_font_file)


def load_bitmap_font(filepath):
    from PIL import ImageFont

    # Convert bpf and pcf files to pillow fonts, first.
    pil_font_path = convert_bitmap_to_pillow_font(filepath)

    try:
        font = ImageFont.load(str(pil_font_path))
    except Exception as e:
        raise core.EsphomeError(
            f"Failed to load bitmap font file: {pil_font_path} : {e}"
        )

    return BitmapFontWrapper(font)


def load_ttf_font(path, size):
    from PIL import ImageFont

    try:
        font = ImageFont.truetype(str(path), size)
    except Exception as e:
        raise core.EsphomeError(f"Could not load truetype file {path}: {e}")

    return TrueTypeFontWrapper(font)


class GlyphInfo:
    def __init__(self, data_len, offset_x, offset_y, width, height):
        self.data_len = data_len
        self.offset_x = offset_x
        self.offset_y = offset_y
        self.width = width
        self.height = height


async def to_code(config):
    glyph_to_font_map = {}
    font_list = font_map[config[CONF_ID]]
    glyphs = []
    for font in font_list:
        glyphs.extend(font.glyphs)
        for glyph in font.glyphs:
            glyph_to_font_map[glyph] = font
    glyphs.sort(key=functools.cmp_to_key(glyph_comparator))
    glyph_args = {}
    data = []
    bpp = config[CONF_BPP]
    if bpp == 1:
        mode = "1"
        scale = 1
    else:
        mode = "L"
        scale = 256 // (1 << bpp)
    for glyph in glyphs:
        font = glyph_to_font_map[glyph].font
        mask = font.getmask(glyph, mode=mode)
        offset_x, offset_y = font.getoffset(glyph)
        width, height = mask.size
        glyph_data = [0] * ((height * width * bpp + 7) // 8)
        pos = 0
        for y in range(height):
            for x in range(width):
                pixel = mask.getpixel((x, y)) // scale
                for bit_num in range(bpp):
                    if pixel & (1 << (bpp - bit_num - 1)):
                        glyph_data[pos // 8] |= 0x80 >> (pos % 8)
                    pos += 1
        glyph_args[glyph] = GlyphInfo(len(data), offset_x, offset_y, width, height)
        data += glyph_data

    rhs = [HexInt(x) for x in data]
    prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)

    glyph_initializer = []
    for glyph in glyphs:
        glyph_initializer.append(
            cg.StructInitializer(
                GlyphData,
                (
                    "a_char",
                    cg.RawExpression(f"(const uint8_t *){cpp_string_escape(glyph)}"),
                ),
                (
                    "data",
                    cg.RawExpression(
                        f"{str(prog_arr)} + {str(glyph_args[glyph].data_len)}"
                    ),
                ),
                ("offset_x", glyph_args[glyph].offset_x),
                ("offset_y", glyph_args[glyph].offset_y),
                ("width", glyph_args[glyph].width),
                ("height", glyph_args[glyph].height),
            )
        )

    glyphs = cg.static_const_array(config[CONF_RAW_GLYPH_ID], glyph_initializer)

    cg.new_Pvariable(
        config[CONF_ID],
        glyphs,
        len(glyph_initializer),
        font_list[0].ascent,
        font_list[0].ascent + font_list[0].descent,
        bpp,
    )
