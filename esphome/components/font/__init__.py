import logging

import functools
from pathlib import Path
import os
import re
from packaging import version
import requests

from esphome import core
from esphome import external_files, helpers
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.helpers import copy_file_if_changed
from esphome.const import (
    CONF_FAMILY,
    CONF_FILE,
    CONF_GLYPHS,
    CONF_ID,
    CONF_RAW_DATA_ID,
    CONF_TYPE,
    CONF_REFRESH,
    CONF_SIZE,
    CONF_PATH,
    CONF_WEIGHT,
    CONF_URL,
)
from esphome.core import CORE, HexInt

_LOGGER = logging.getLogger(__name__)

DOMAIN = "font"
DEPENDENCIES = ["display"]
MULTI_CONF = True

font_ns = cg.esphome_ns.namespace("font")

Font = font_ns.class_("Font")
Glyph = font_ns.class_("Glyph")
GlyphData = font_ns.struct("GlyphData")


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
            '(pip install "pillow==10.1.0")'
        ) from err

    if version.parse(PIL.__version__) != version.parse("10.1.0"):
        raise cv.Invalid(
            "Please update your pillow installation to 10.1.0. "
            '(pip install "pillow==10.1.0")'
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


def get_font_url(value):
    if value[CONF_TYPE] == TYPE_GFONTS:
        name = get_font_name(value)
        return f"https://fonts.googleapis.com/css2?family={name}"
    if value[CONF_TYPE] == TYPE_WEB:
        return value[CONF_URL]
    return None


def get_font_name(value):
    if value[CONF_TYPE] == TYPE_GFONTS:
        return f"{value[CONF_FAMILY]}:ital,wght@{int(value[CONF_ITALIC])},{value[CONF_WEIGHT]}"
    if value[CONF_TYPE] == TYPE_WEB:
        file_name, _, _ = external_files.get_file_info_from_url(value[CONF_URL])
        return file_name
    return None


def get_font_path(value):
    if value[CONF_TYPE] == TYPE_GFONTS:
        name = f"{value[CONF_FAMILY]}@{value[CONF_WEIGHT]}@{value[CONF_ITALIC]}@v1"
        return external_files.compute_local_file_dir(name, DOMAIN) / "font.ttf"
    if value[CONF_TYPE] == TYPE_WEB:
        file_name, file_extension, temp_path = external_files.get_file_info_from_url(
            value[CONF_URL]
        )
        if temp_path is not None:
            helpers.delete_file(temp_path)
        _LOGGER.debug("get_font_path: file_name=%s", file_name)
        name = f"{file_name}@{value[CONF_WEIGHT]}@{value[CONF_ITALIC]}"
        file_path = Path(external_files.compute_local_file_dir(name, DOMAIN))
        output_file_name = f"{file_name}{file_extension}"
        _LOGGER.debug("get_font_path: file_path=%s", file_path / output_file_name)
        return file_path / output_file_name
    return None


def download_gfont_ttf(value, url):
    try:
        req = requests.get(url, timeout=external_files.NETWORK_TIMEOUT)
        req.raise_for_status()
    except requests.exceptions.RequestException as e:
        raise cv.Invalid(
            f"Could not download font at {url}, please check the fonts exists "
            f"at google fonts ({e})"
        )
    match = re.search(r"src:\s+url\((.+)\)\s+format\('truetype'\);", req.text)
    name = get_font_name(value)
    if match is None:
        raise cv.Invalid(
            f"Could not extract ttf file from gfonts response for {name}, "
            f"please report this."
        )

    ttf_url = match.group(1)
    try:
        req = requests.get(ttf_url, timeout=external_files.NETWORK_TIMEOUT)
        req.raise_for_status()
        return req.content
    except requests.exceptions.RequestException as e:
        raise cv.Invalid(f"Could not download ttf file for {name} ({ttf_url}): {e}")


def download_web_font(value):
    name = get_font_name(value)
    url = get_font_url(value)
    path = get_font_path(value)

    if external_files.is_file_recent(
        path, value[CONF_REFRESH]
    ) or not external_files.has_remote_file_changed(url, path):
        return value

    if value[CONF_TYPE] == TYPE_WEB:
        try:
            req = requests.get(url, timeout=external_files.NETWORK_TIMEOUT)
            req.raise_for_status()
            path.parent.mkdir(exist_ok=True, parents=True)
            path.write_bytes(req.content)
        except requests.exceptions.RequestException as e:
            raise cv.Invalid(
                f"Could not download font for {name}, please check the fonts exists "
                f"at google fonts ({e})"
            )

    elif value[CONF_TYPE] == TYPE_GFONTS:
        content = download_gfont_ttf(value, url)
        path.parent.mkdir(exist_ok=True, parents=True)
        path.write_bytes(content)
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

GFONTS_SCHEMA = EXTERNAL_FONT_SCHEMA.extend(
    {cv.Required(CONF_FAMILY): cv.string_strict}
)

WEB_FONT_SCHEMA = EXTERNAL_FONT_SCHEMA.extend({cv.Required(CONF_URL): cv.string_strict})


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
    typed_schema = TYPED_FILE_SCHEMA(value)
    if typed_schema[CONF_TYPE] == TYPE_WEB or typed_schema[CONF_TYPE] == TYPE_GFONTS:
        download_web_font(typed_schema)
    return typed_schema


FILE_SCHEMA = cv.All(_file_schema)


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
    },
)

CONFIG_SCHEMA = cv.All(validate_pillow_installed, FONT_SCHEMA)

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
            if height > max_height:
                max_height = height
        return (max_height, 0)


def convert_bitmap_to_pillow_font(filepath):
    from PIL import PcfFontFile, BdfFontFile

    local_bitmap_font_file = external_files.compute_local_file_dir(
        filepath,
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


async def to_code(config):
    conf = config[CONF_FILE]
    if conf[CONF_TYPE] == TYPE_LOCAL_BITMAP:
        font = load_bitmap_font(CORE.relative_config_path(conf[CONF_PATH]))
    elif conf[CONF_TYPE] == TYPE_LOCAL:
        path = CORE.relative_config_path(conf[CONF_PATH])
        font = load_ttf_font(path, config[CONF_SIZE])
    elif conf[CONF_TYPE] == TYPE_GFONTS or conf[CONF_TYPE] == TYPE_WEB:
        path = get_font_path(conf)
        font = load_ttf_font(path, config[CONF_SIZE])
    else:
        raise core.EsphomeError(f"Could not load font: unknown type: {conf[CONF_TYPE]}")

    ascent, descent = font.getmetrics(config[CONF_GLYPHS])

    glyph_args = {}
    data = []
    for glyph in config[CONF_GLYPHS]:
        mask = font.getmask(glyph, mode="1")
        offset_x, offset_y = font.getoffset(glyph)
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
