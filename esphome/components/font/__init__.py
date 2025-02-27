from collections.abc import MutableMapping
import functools
import hashlib
import logging
import os
from pathlib import Path
import re

import esphome_glyphsets as glyphsets
from freetype import Face, ft_pixel_mode_grays, ft_pixel_mode_mono
import requests

from esphome import external_files
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
from esphome.helpers import cpp_string_escape

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
class FontCache(MutableMapping):
    @staticmethod
    def get_name(value):
        if CONF_FAMILY in value:
            return (
                f"{value[CONF_FAMILY]}:{int(value[CONF_ITALIC])}:{value[CONF_WEIGHT]}"
            )
        if CONF_URL in value:
            return value[CONF_URL]
        return value[CONF_PATH]

    @staticmethod
    def _keytransform(value):
        if CONF_FAMILY in value:
            return f"gfont:{value[CONF_FAMILY]}:{int(value[CONF_ITALIC])}:{value[CONF_WEIGHT]}"
        if CONF_URL in value:
            return f"url:{value[CONF_URL]}"
        return f"file:{value[CONF_PATH]}"

    def __init__(self):
        self.store = {}

    def __delitem__(self, key):
        del self.store[self._keytransform(key)]

    def __iter__(self):
        return iter(self.store)

    def __len__(self):
        return len(self.store)

    def __getitem__(self, item):
        return self.store[self._keytransform(item)]

    def __setitem__(self, key, value):
        self.store[self._keytransform(key)] = Face(str(value))


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


def check_missing_glyphs(file, codepoints, warning: bool = False):
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
        message = f"Font {FontCache.get_name(file)} is missing {count} glyph{'s' if count != 1 else ''}:\n    {missing_str}"
        if warning:
            _LOGGER.warning(message)
        else:
            raise cv.Invalid(message)


def validate_font_config(config):
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
        if any(x >= 256 for x in setpoints.copy().union(glyphspoints)):
            raise cv.Invalid("Codepoints in bitmap fonts must be in the range 0-255")
    else:
        # for TT fonts, check that glyphs are actually present
        # Check extras against their own font, exclude from parent font codepoints
        for extra in config[CONF_EXTRAS]:
            points = {ord(x) for x in flatten(extra[CONF_GLYPHS])}
            glyphspoints.difference_update(points)
            setpoints.difference_update(points)
            check_missing_glyphs(extra[CONF_FILE], points)

        # A named glyph that can't be provided is an error

        check_missing_glyphs(fileconf, glyphspoints)
        # A missing glyph from a set is a warning.
        if not config[CONF_IGNORE_MISSING_GLYPHS]:
            check_missing_glyphs(fileconf, setpoints, warning=True)

    # Populate the default after the above checks so that use of the default doesn't trigger errors
    if not config[CONF_GLYPHS] and not config[CONF_GLYPHSETS]:
        if fileconf[CONF_TYPE] == TYPE_LOCAL_BITMAP:
            config[CONF_GLYPHS] = [DEFAULT_GLYPHS]
        else:
            # set a default glyphset, intersected with what the font actually offers
            font = FONT_CACHE[fileconf]
            config[CONF_GLYPHS] = [
                chr(x)
                for x in glyphsets.unicodes_per_glyphset(DEFAULT_GLYPHSET)
                if font.get_char_index(x) != 0
            ]

    if config[CONF_FILE][CONF_TYPE] == TYPE_LOCAL_BITMAP:
        if CONF_SIZE in config:
            raise cv.Invalid(
                "Size is not a valid option for bitmap fonts, which are inherently fixed size"
            )
    elif CONF_SIZE not in config:
        config[CONF_SIZE] = 20

    return config


FONT_EXTENSIONS = (".ttf", ".woff", ".otf")
BITMAP_EXTENSIONS = (".bdf", ".pcf")


def validate_bitmap_file(value):
    if not any(map(value.lower().endswith, BITMAP_EXTENSIONS)):
        raise cv.Invalid(f"Only {', '.join(BITMAP_EXTENSIONS)} files are supported.")
    return CORE.relative_config_path(cv.file_(value))


def validate_truetype_file(value):
    if value.lower().endswith(".zip"):  # for Google Fonts downloads
        raise cv.Invalid(
            f"Please unzip the font archive '{value}' first and then use the .ttf files inside."
        )
    if not any(map(value.lower().endswith, FONT_EXTENSIONS)):
        raise cv.Invalid(f"Only {', '.join(FONT_EXTENSIONS)} files are supported.")
    return CORE.relative_config_path(cv.file_(value))


def add_local_file(value):
    if value in FONT_CACHE:
        return value
    path = value[CONF_PATH]
    if not os.path.isfile(path):
        raise cv.Invalid(f"File '{path}' not found.")
    FONT_CACHE[value] = path
    return value


TYPE_LOCAL = "local"
TYPE_LOCAL_BITMAP = "local_bitmap"
TYPE_GFONTS = "gfonts"
TYPE_WEB = "web"
LOCAL_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_PATH): validate_truetype_file,
        }
    ),
    add_local_file,
)

LOCAL_BITMAP_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_PATH): validate_bitmap_file,
        }
    ),
    add_local_file,
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
    _LOGGER.debug("_compute_local_font_path: %s", base_dir / key)
    return base_dir / key


def download_gfont(value):
    if value in FONT_CACHE:
        return value
    name = (
        f"{value[CONF_FAMILY]}:ital,wght@{int(value[CONF_ITALIC])},{value[CONF_WEIGHT]}"
    )
    url = f"https://fonts.googleapis.com/css2?family={name}"
    path = (
        external_files.compute_local_file_dir(DOMAIN)
        / f"{value[CONF_FAMILY]}@{value[CONF_WEIGHT]}@{value[CONF_ITALIC]}@v1.ttf"
    )
    if not external_files.is_file_recent(str(path), value[CONF_REFRESH]):
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
        # In case the remote file is not modified, the download_content function will return the existing file,
        # so update the modification time to now.
        path.touch()
    FONT_CACHE[value] = path
    return value


def download_web_font(value):
    if value in FONT_CACHE:
        return value
    url = value[CONF_URL]
    path = _compute_local_font_path(value) / "font.ttf"

    external_files.download_content(url, path)
    _LOGGER.debug("download_web_font: path=%s", path)
    FONT_CACHE[value] = path
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
        return font_file_schema(data)

    if value.startswith("http://") or value.startswith("https://"):
        return font_file_schema(
            {
                CONF_TYPE: TYPE_WEB,
                CONF_URL: value,
            }
        )

    extension = Path(value).suffix
    if extension in BITMAP_EXTENSIONS:
        return font_file_schema(
            {
                CONF_TYPE: TYPE_LOCAL_BITMAP,
                CONF_PATH: value,
            }
        )

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
DEFAULT_GLYPHS = (
    ' !"%()+=,-.:/?0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz°'
)

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
        cv.Optional(CONF_SIZE): cv.int_range(min=1),
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

CONFIG_SCHEMA = cv.All(FONT_SCHEMA, validate_font_config)


class EFont:
    def __init__(self, file, codepoints):
        self.codepoints = codepoints
        self.font: Face = FONT_CACHE[file]


class GlyphInfo:
    def __init__(self, data_len, advance, offset_x, offset_y, width, height):
        self.data_len = data_len
        self.advance = advance
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
    # Create the codepoint to font file map
    base_font = FONT_CACHE[config[CONF_FILE]]
    point_font_map: dict[str, Face] = {c: base_font for c in point_set}
    # process extras, updating the map and extending the codepoint list
    for extra in config[CONF_EXTRAS]:
        extra_points = flatten(extra[CONF_GLYPHS])
        point_set.update(extra_points)
        extra_font = FONT_CACHE[extra[CONF_FILE]]
        point_font_map.update({c: extra_font for c in extra_points})

    codepoints = list(point_set)
    codepoints.sort(key=functools.cmp_to_key(glyph_comparator))
    glyph_args = {}
    data = []
    bpp = config[CONF_BPP]
    mode = ft_pixel_mode_grays
    scale = 256 // (1 << bpp)
    # create the data array for all glyphs
    for codepoint in codepoints:
        font = point_font_map[codepoint]
        if not font.has_fixed_sizes:
            font.set_pixel_sizes(config[CONF_SIZE], 0)
        font.load_char(codepoint)
        font.glyph.render(mode)
        width = font.glyph.bitmap.width
        height = font.glyph.bitmap.rows
        buffer = font.glyph.bitmap.buffer
        pitch = font.glyph.bitmap.pitch
        glyph_data = [0] * ((height * width * bpp + 7) // 8)
        src_mode = font.glyph.bitmap.pixel_mode
        pos = 0
        for y in range(height):
            for x in range(width):
                if src_mode == ft_pixel_mode_mono:
                    pixel = (
                        (1 << bpp) - 1
                        if buffer[y * pitch + x // 8] & (1 << (7 - x % 8))
                        else 0
                    )
                else:
                    pixel = buffer[y * pitch + x] // scale
                for bit_num in range(bpp):
                    if pixel & (1 << (bpp - bit_num - 1)):
                        glyph_data[pos // 8] |= 0x80 >> (pos % 8)
                    pos += 1
        ascender = font.size.ascender // 64
        if ascender == 0:
            if font.has_fixed_sizes:
                ascender = font.available_sizes[0].height
            else:
                _LOGGER.error(
                    "Unable to determine ascender of font %s", config[CONF_FILE]
                )
        glyph_args[codepoint] = GlyphInfo(
            len(data),
            font.glyph.metrics.horiAdvance // 64,
            font.glyph.bitmap_left,
            ascender - font.glyph.bitmap_top,
            width,
            height,
        )
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
                ("advance", glyph_args[codepoint].advance),
                ("offset_x", glyph_args[codepoint].offset_x),
                ("offset_y", glyph_args[codepoint].offset_y),
                ("width", glyph_args[codepoint].width),
                ("height", glyph_args[codepoint].height),
            )
        )

    glyphs = cg.static_const_array(config[CONF_RAW_GLYPH_ID], glyph_initializer)

    font_height = base_font.size.height // 64
    ascender = base_font.size.ascender // 64
    if font_height == 0:
        if base_font.has_fixed_sizes:
            font_height = base_font.available_sizes[0].height
            ascender = font_height
        else:
            _LOGGER.error("Unable to determine height of font %s", config[CONF_FILE])
    cg.new_Pvariable(
        config[CONF_ID],
        glyphs,
        len(glyph_initializer),
        ascender,
        font_height,
        bpp,
    )
