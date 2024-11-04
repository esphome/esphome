from collections.abc import Iterable
import functools
import hashlib
import logging
import os
from pathlib import Path
import re

import freetype
import glyphsets
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
CONF_GLYPHSETS = "glyphsets"
CONF_IGNORE_MISSING_GLYPHS = "ignore_missing_glyphs"


# Cache loaded freetype fonts
class FontCache(dict):
    def __missing__(self, key):
        res = self[key] = freetype.Face(key)
        return res


FONT_CACHE = FontCache()


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
    return 0


def flatten(lists) -> list:
    """
    Given a list of lists, flatten it to a single list of all elements of all lists.
    This wraps itertools.chain.from_iterable to make it more readable, and return a list
    rather than a single use iterable.
    """
    from itertools import chain

    return list(chain.from_iterable(lists))


def check_missing_glyphs(file, codepoints: Iterable, warning: bool = False):
    """
    Check that the given font file actually contains the requested glyphs
    :param file: A Truetype font file
    :param codepoints: A list of codepoints to check
    :param warning: If true, log a warning instead of raising an exception
    """

    font = FONT_CACHE[file]
    missing = [chr(x) for x in codepoints if font.get_char_index(x) == 0]
    if missing:
        # Only list up to 10 missing glyphs
        missing.sort(key=functools.cmp_to_key(glyph_comparator))
        count = len(missing)
        missing = missing[:10]
        missing_str = "\n    ".join(
            f"{x} ({x.encode('unicode_escape')})" for x in missing
        )
        if count > 10:
            missing_str += f"\n    and {count - 10} more."
        message = f"Font {Path(file).name} is missing {count} glyph{'s' if count != 1 else ''}:\n    {missing_str}"
        if warning:
            _LOGGER.warning(message)
        else:
            raise cv.Invalid(message)


def validate_glyphs(config):
    """
    Check for duplicate codepoints, then check that all requested codepoints actually
    have glyphs defined in the appropriate font file.
    """

    # Collect all glyph codepoints and flatten to a list of chars
    glyphspoints = flatten(
        [x[CONF_GLYPHS] for x in config[CONF_EXTRAS]] + config[CONF_GLYPHS]
    )
    # Convert a list of strings to a list of chars (one char strings)
    glyphspoints = flatten([list(x) for x in glyphspoints])
    if len(set(glyphspoints)) != len(glyphspoints):
        duplicates = {x for x in glyphspoints if glyphspoints.count(x) > 1}
        dup_str = ", ".join(f"{x} ({x.encode('unicode_escape')})" for x in duplicates)
        raise cv.Invalid(
            f"Found duplicate glyph{'s' if len(duplicates) != 1 else ''}: {dup_str}"
        )
    # convert to codepoints
    glyphspoints = {ord(x) for x in glyphspoints}
    fileconf = config[CONF_FILE]
    setpoints = set(
        flatten([glyphsets.unicodes_per_glyphset(x) for x in config[CONF_GLYPHSETS]])
    )
    # Make setpoints and glyphspoints disjoint
    setpoints.difference_update(glyphspoints)
    if fileconf[CONF_TYPE] == TYPE_LOCAL_BITMAP:
        # Pillow only allows 256 glyphs per bitmap font. Not sure if that is a Pillow limitation
        # or a file format limitation
        if any(x >= 256 for x in setpoints.copy().union(glyphspoints)):
            raise cv.Invalid("Codepoints in bitmap fonts must be in the range 0-255")
    else:
        # for TT fonts, check that glyphs are actually present
        # Check extras against their own font, exclude from parent font codepoints
        for extra in config[CONF_EXTRAS]:
            points = {ord(x) for x in flatten(extra[CONF_GLYPHS])}
            glyphspoints.difference_update(points)
            setpoints.difference_update(points)
            check_missing_glyphs(extra[CONF_FILE][CONF_PATH], points)

        # A named glyph that can't be provided is an error
        check_missing_glyphs(fileconf[CONF_PATH], glyphspoints)
        # A missing glyph from a set is a warning.
        if not config[CONF_IGNORE_MISSING_GLYPHS]:
            check_missing_glyphs(fileconf[CONF_PATH], setpoints, warning=True)

    # Populate the default after the above checks so that use of the default doesn't trigger errors
    if not config[CONF_GLYPHS] and not config[CONF_GLYPHSETS]:
        if fileconf[CONF_TYPE] == TYPE_LOCAL_BITMAP:
            config[CONF_GLYPHS] = [DEFAULT_GLYPHS]
        else:
            # set a default glyphset, intersected with what the font actually offers
            font = FONT_CACHE[fileconf[CONF_PATH]]
            config[CONF_GLYPHS] = [
                chr(x)
                for x in glyphsets.unicodes_per_glyphset(DEFAULT_GLYPHSET)
                if font.get_char_index(x) != 0
            ]

    return config


def validate_pillow_installed(value):
    try:
        import PIL
    except ImportError as err:
        raise cv.Invalid(
            "Please install the pillow python package to use this feature. "
            '(pip install "pillow==10.4.0")'
        ) from err

    if version.parse(PIL.__version__) != version.parse("10.4.0"):
        raise cv.Invalid(
            "Please update your pillow installation to 10.4.0. "
            '(pip install "pillow==10.4.0")'
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
    return CORE.relative_config_path(cv.file_(value))


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

FULLPATH_SCHEMA = cv.maybe_simple_value(
    {cv.Required(CONF_PATH): cv.string}, key=CONF_PATH
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


def get_font_path(value, font_type) -> Path:
    if font_type == TYPE_GFONTS:
        name = f"{value[CONF_FAMILY]}@{value[CONF_WEIGHT]}@{value[CONF_ITALIC]}@v1"
        return external_files.compute_local_file_dir(DOMAIN) / f"{name}.ttf"
    if font_type == TYPE_WEB:
        return _compute_local_font_path(value) / "font.ttf"
    assert False


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
    return FULLPATH_SCHEMA(path)


def download_web_font(value):
    url = value[CONF_URL]
    path = get_font_path(value, TYPE_WEB)

    external_files.download_content(url, path)
    _LOGGER.debug("download_web_font: path=%s", path)
    return FULLPATH_SCHEMA(path)


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
        return font_file_schema(data)

    if value.startswith("http://") or value.startswith("https://"):
        return font_file_schema(
            {
                CONF_TYPE: TYPE_WEB,
                CONF_URL: value,
            }
        )

    if value.endswith(".pcf") or value.endswith(".bdf"):
        value = convert_bitmap_to_pillow_font(
            CORE.relative_config_path(cv.file_(value))
        )
        return {
            CONF_TYPE: TYPE_LOCAL_BITMAP,
            CONF_PATH: value,
        }

    return font_file_schema(
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


def font_file_schema(value):
    if isinstance(value, str):
        return validate_file_shorthand(value)
    return TYPED_FILE_SCHEMA(value)


# Default if no glyphs or glyphsets are provided
DEFAULT_GLYPHSET = "GF_Latin_Kernel"
# default for bitmap fonts
DEFAULT_GLYPHS = ' !"%()+=,-.:/?0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz<C2><B0>'

CONF_RAW_GLYPH_ID = "raw_glyph_id"

FONT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(Font),
        cv.Required(CONF_FILE): font_file_schema,
        cv.Optional(CONF_GLYPHS, default=[]): cv.ensure_list(cv.string_strict),
        cv.Optional(CONF_GLYPHSETS, default=[]): cv.ensure_list(
            cv.one_of(*glyphsets.defined_glyphsets())
        ),
        cv.Optional(CONF_IGNORE_MISSING_GLYPHS, default=False): cv.boolean,
        cv.Optional(CONF_SIZE, default=20): cv.int_range(min=1),
        cv.Optional(CONF_BPP, default=1): cv.one_of(1, 2, 4, 8),
        cv.Optional(CONF_EXTRAS, default=[]): cv.ensure_list(
            cv.Schema(
                {
                    cv.Required(CONF_FILE): font_file_schema,
                    cv.Required(CONF_GLYPHS): cv.ensure_list(cv.string_strict),
                }
            )
        ),
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        cv.GenerateID(CONF_RAW_GLYPH_ID): cv.declare_id(GlyphData),
    },
)

CONFIG_SCHEMA = cv.All(validate_pillow_installed, FONT_SCHEMA, validate_glyphs)


# PIL doesn't provide a consistent interface for both TrueType and bitmap
# fonts. So, we use our own wrappers to give us the consistency that we need.


class TrueTypeFontWrapper:
    def __init__(self, font):
        self.font = font

    def getoffset(self, glyph):
        _, (offset_x, offset_y) = self.font.font.getsize(glyph)
        return offset_x, offset_y

    def getmask(self, glyph, **kwargs):
        return self.font.getmask(str(glyph), **kwargs)

    def getmetrics(self, glyphs):
        return self.font.getmetrics()


class BitmapFontWrapper:
    def __init__(self, font):
        self.font = font
        self.max_height = 0

    def getoffset(self, glyph):
        return 0, 0

    def getmask(self, glyph, **kwargs):
        return self.font.getmask(str(glyph), **kwargs)

    def getmetrics(self, glyphs):
        max_height = 0
        for glyph in glyphs:
            mask = self.getmask(glyph, mode="1")
            _, height = mask.size
            max_height = max(max_height, height)
        return max_height, 0


class EFont:
    def __init__(self, file, size, codepoints):
        self.codepoints = codepoints
        path = file[CONF_PATH]
        self.name = Path(path).name
        ftype = file[CONF_TYPE]
        if ftype == TYPE_LOCAL_BITMAP:
            self.font = load_bitmap_font(path)
        else:
            self.font = load_ttf_font(path, size)
        self.ascent, self.descent = self.font.getmetrics(codepoints)


def convert_bitmap_to_pillow_font(filepath):
    from PIL import BdfFontFile, PcfFontFile

    local_bitmap_font_file = external_files.compute_local_file_dir(
        DOMAIN,
    ) / os.path.basename(filepath)

    copy_file_if_changed(filepath, local_bitmap_font_file)

    local_pil_font_file = local_bitmap_font_file.with_suffix(".pil")
    with open(local_bitmap_font_file, "rb") as fp:
        try:
            try:
                p = PcfFontFile.PcfFontFile(fp)
            except SyntaxError:
                fp.seek(0)
                p = BdfFontFile.BdfFontFile(fp)

            # Convert to pillow-formatted fonts, which have a .pil and .pbm extension.
            p.save(local_pil_font_file)
        except (SyntaxError, OSError) as err:
            raise core.EsphomeError(
                f"Failed to parse as bitmap font: '{filepath}': {err}"
            )

    return str(local_pil_font_file)


def load_bitmap_font(filepath):
    from PIL import ImageFont

    try:
        font = ImageFont.load(str(filepath))
    except Exception as e:
        raise core.EsphomeError(f"Failed to load bitmap font file: {filepath}: {e}")

    return BitmapFontWrapper(font)


def load_ttf_font(path, size):
    from PIL import ImageFont

    try:
        font = ImageFont.truetype(str(path), size)
    except Exception as e:
        raise core.EsphomeError(f"Could not load TrueType file {path}: {e}")

    return TrueTypeFontWrapper(font)


class GlyphInfo:
    def __init__(self, data_len, offset_x, offset_y, width, height):
        self.data_len = data_len
        self.offset_x = offset_x
        self.offset_y = offset_y
        self.width = width
        self.height = height


async def to_code(config):
    """
    Collect all glyph codepoints, construct a map from a codepoint to a font file.
    Codepoints are either explicit (glyphs key in top level or extras) or part of a glyphset.
    Codepoints listed in extras use the extra font and override codepoints from glyphsets.
    Achieve this by processing the base codepoints first, then the extras
    """

    # get the codepoints from glyphsets and flatten to a set of chrs.
    point_set: set[str] = {
        chr(x)
        for x in flatten(
            [glyphsets.unicodes_per_glyphset(x) for x in config[CONF_GLYPHSETS]]
        )
    }
    # get the codepoints from the glyphs key, flatten to a list of chrs and combine with the points from glyphsets
    point_set.update(flatten(config[CONF_GLYPHS]))
    size = config[CONF_SIZE]
    # Create the codepoint to font file map
    base_font = EFont(config[CONF_FILE], size, point_set)
    point_font_map: dict[str, EFont] = {c: base_font for c in point_set}
    # process extras, updating the map and extending the codepoint list
    for extra in config[CONF_EXTRAS]:
        extra_points = flatten(extra[CONF_GLYPHS])
        point_set.update(extra_points)
        extra_font = EFont(extra[CONF_FILE], size, extra_points)
        point_font_map.update({c: extra_font for c in extra_points})

    codepoints = list(point_set)
    codepoints.sort(key=functools.cmp_to_key(glyph_comparator))
    glyph_args = {}
    data = []
    bpp = config[CONF_BPP]
    if bpp == 1:
        mode = "1"
        scale = 1
    else:
        mode = "L"
        scale = 256 // (1 << bpp)
    # create the data array for all glyphs
    for codepoint in codepoints:
        font = point_font_map[codepoint]
        mask = font.font.getmask(codepoint, mode=mode)
        offset_x, offset_y = font.font.getoffset(codepoint)
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
        glyph_args[codepoint] = GlyphInfo(len(data), offset_x, offset_y, width, height)
        data += glyph_data

    rhs = [HexInt(x) for x in data]
    prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)

    # Create the glyph table that points to data in the above array.
    glyph_initializer = []
    for codepoint in codepoints:
        glyph_initializer.append(
            cg.StructInitializer(
                GlyphData,
                (
                    "a_char",
                    cg.RawExpression(
                        f"(const uint8_t *){cpp_string_escape(codepoint)}"
                    ),
                ),
                (
                    "data",
                    cg.RawExpression(
                        f"{str(prog_arr)} + {str(glyph_args[codepoint].data_len)}"
                    ),
                ),
                ("offset_x", glyph_args[codepoint].offset_x),
                ("offset_y", glyph_args[codepoint].offset_y),
                ("width", glyph_args[codepoint].width),
                ("height", glyph_args[codepoint].height),
            )
        )

    glyphs = cg.static_const_array(config[CONF_RAW_GLYPH_ID], glyph_initializer)

    cg.new_Pvariable(
        config[CONF_ID],
        glyphs,
        len(glyph_initializer),
        base_font.ascent,
        base_font.ascent + base_font.descent,
        bpp,
    )
