import functools
from pathlib import Path
import hashlib
import re

import requests

from esphome import core
from esphome.components import display
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_FAMILY,
    CONF_FILE,
    CONF_GLYPHS,
    CONF_ID,
    CONF_RAW_DATA_ID,
    CONF_TYPE,
    CONF_SIZE,
    CONF_PATH,
    CONF_WEIGHT,
)
from esphome.core import CORE, HexInt


DOMAIN = "font"
DEPENDENCIES = ["display"]
MULTI_CONF = True

Font = display.display_ns.class_("Font")
Glyph = display.display_ns.class_("Glyph")
GlyphData = display.display_ns.struct("GlyphData")


def validate_glyphs(value):
    if isinstance(value, list):
        value = cv.Schema([cv.string])(value)
    value = cv.Schema([cv.string])(list(value))

    def comparator(x, y):
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

    value.sort(key=functools.cmp_to_key(comparator))
    return value


def validate_pillow_installed(value):
    try:
        import PIL
    except ImportError as err:
        raise cv.Invalid(
            "Please install the pillow python package to use this feature. "
            "(pip install pillow)"
        ) from err

    if PIL.__version__[0] < "4":
        raise cv.Invalid(
            "Please update your pillow installation to at least 4.0.x. "
            "(pip install -U pillow)"
        )

    return value


def validate_truetype_file(value):
    if value.endswith(".zip"):  # for Google Fonts downloads
        raise cv.Invalid(
            f"Please unzip the font archive '{value}' first and then use the .ttf files inside."
        )
    if not value.endswith(".ttf"):
        raise cv.Invalid(
            "Only truetype (.ttf) files are supported. Please make sure you're "
            "using the correct format or rename the extension to .ttf"
        )
    return cv.file_(value)


def _compute_gfonts_local_path(value) -> Path:
    name = f"{value[CONF_FAMILY]}@{value[CONF_WEIGHT]}@{value[CONF_ITALIC]}@v1"
    base_dir = Path(CORE.config_dir) / ".esphome" / DOMAIN
    h = hashlib.new("sha256")
    h.update(name.encode())
    return base_dir / h.hexdigest()[:8] / "font.ttf"


TYPE_LOCAL = "local"
TYPE_GFONTS = "gfonts"
LOCAL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PATH): validate_truetype_file,
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


def download_gfonts(value):
    wght = value[CONF_WEIGHT]
    if value[CONF_ITALIC]:
        wght = f"1,{wght}"
    name = f"{value[CONF_FAMILY]}@{value[CONF_WEIGHT]}"
    url = f"https://fonts.googleapis.com/css2?family={value[CONF_FAMILY]}:wght@{wght}"

    path = _compute_gfonts_local_path(value)
    if path.is_file():
        return value
    try:
        req = requests.get(url)
        req.raise_for_status()
    except requests.exceptions.RequestException as e:
        raise cv.Invalid(
            f"Could not download font for {name}, please check the fonts exists "
            f"at google fonts ({e})"
        )
    match = re.search(r"src:\s+url\((.+)\)\s+format\('truetype'\);", req.text)
    if match is None:
        raise cv.Invalid(
            f"Could not extract ttf file from gfonts response for {name}, "
            f"please report this."
        )

    ttf_url = match.group(1)
    try:
        req = requests.get(ttf_url)
        req.raise_for_status()
    except requests.exceptions.RequestException as e:
        raise cv.Invalid(f"Could not download ttf file for {name} ({ttf_url}): {e}")

    path.parent.mkdir(exist_ok=True, parents=True)
    path.write_bytes(req.content)
    return value


GFONTS_SCHEMA = cv.All(
    {
        cv.Required(CONF_FAMILY): cv.string_strict,
        cv.Optional(CONF_WEIGHT, default="regular"): cv.Any(
            cv.int_, validate_weight_name
        ),
        cv.Optional(CONF_ITALIC, default=False): cv.boolean,
    },
    download_gfonts,
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
    }
)


def _file_schema(value):
    if isinstance(value, str):
        return validate_file_shorthand(value)
    return TYPED_FILE_SCHEMA(value)


FILE_SCHEMA = cv.Schema(_file_schema)


DEFAULT_GLYPHS = (
    ' !"%()+=,-.:/0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz°'
)
CONF_RAW_GLYPH_ID = "raw_glyph_id"

FONT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(Font),
        cv.Required(CONF_FILE): FILE_SCHEMA,
        cv.Optional(CONF_GLYPHS, default=DEFAULT_GLYPHS): validate_glyphs,
        cv.Optional(CONF_SIZE, default=20): cv.int_range(min=1),
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        cv.GenerateID(CONF_RAW_GLYPH_ID): cv.declare_id(GlyphData),
    }
)

CONFIG_SCHEMA = cv.All(validate_pillow_installed, FONT_SCHEMA)


async def to_code(config):
    from PIL import ImageFont

    conf = config[CONF_FILE]
    if conf[CONF_TYPE] == TYPE_LOCAL:
        path = CORE.relative_config_path(conf[CONF_PATH])
    elif conf[CONF_TYPE] == TYPE_GFONTS:
        path = _compute_gfonts_local_path(conf)
    try:
        font = ImageFont.truetype(str(path), config[CONF_SIZE])
    except Exception as e:
        raise core.EsphomeError(f"Could not load truetype file {path}: {e}")

    ascent, descent = font.getmetrics()

    glyph_args = {}
    data = []
    for glyph in config[CONF_GLYPHS]:
        mask = font.getmask(glyph, mode="1")
        _, (offset_x, offset_y) = font.font.getsize(glyph)
        width, height = mask.size
        width8 = ((width + 7) // 8) * 8
        glyph_data = [0] * (height * width8 // 8)
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

    glyph_initializer = []
    for glyph in config[CONF_GLYPHS]:
        glyph_initializer.append(
            cg.StructInitializer(
                GlyphData,
                ("a_char", glyph),
                (
                    "data",
                    cg.RawExpression(f"{str(prog_arr)} + {str(glyph_args[glyph][0])}"),
                ),
                ("offset_x", glyph_args[glyph][1]),
                ("offset_y", glyph_args[glyph][2]),
                ("width", glyph_args[glyph][3]),
                ("height", glyph_args[glyph][4]),
            )
        )

    glyphs = cg.static_const_array(config[CONF_RAW_GLYPH_ID], glyph_initializer)

    cg.new_Pvariable(
        config[CONF_ID], glyphs, len(glyph_initializer), ascent, ascent + descent
    )
