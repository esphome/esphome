from collections.abc import Iterable, MutableMapping
import functools
from itertools import accumulate
import logging
from pathlib import Path
import re

import esphome_glyphsets as glyphsets

# pylint: disable=no-name-in-module
from freetype import (
    FT_LOAD_NO_BITMAP,
    FT_LOAD_RENDER,
    FT_LOAD_TARGET_MONO,
    Face,
    FT_Exception,
    ft_pixel_mode_mono,
)

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
from esphome.external_files import RemoteFile
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

DOMAIN = "font"
MULTI_CONF = True

CODEOWNERS = ["@esphome/core", "@clydebarrow"]

font_ns = cg.esphome_ns.namespace("font")

Font = font_ns.class_("Font")
Glyph = font_ns.class_("Glyph")

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
        transformed = self._keytransform(key)
        try:
            self.store[transformed] = Face(str(value))
        except FT_Exception as exc:
            file = transformed.split(":", 1)
            raise cv.Invalid(
                f"{file[0].capitalize()} {file[1]} is not a valid font file"
            ) from exc


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


def pt_to_px(pt):
    """
    Convert a point size to pixels, rounding up to the nearest pixel
    """
    return (pt + 63) // 64


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
    # check that glyphs are actually present
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
    font = FONT_CACHE[fileconf]
    if not config[CONF_GLYPHS] and not config[CONF_GLYPHSETS]:
        # set a default glyphset, intersected with what the font actually offers
        config[CONF_GLYPHS] = [
            chr(x)
            for x in glyphsets.unicodes_per_glyphset(DEFAULT_GLYPHSET)
            if font.get_char_index(x) != 0
        ]

    if not font.is_scalable:
        sizes = [pt_to_px(x.size) for x in font.available_sizes]
        if not sizes:
            raise cv.Invalid(
                f"Font {FontCache.get_name(fileconf)} has no available sizes"
            )
        if CONF_SIZE not in config:
            config[CONF_SIZE] = sizes[0]
        elif config[CONF_SIZE] not in sizes:
            sizes = ", ".join(str(x) for x in sizes)
            raise cv.Invalid(
                f"Font {FontCache.get_name(fileconf)} only has size{'s' if len(sizes) != 1 else ''} {sizes} available"
            )
    elif CONF_SIZE not in config:
        config[CONF_SIZE] = 20

    return config


FONT_EXTENSIONS = (".ttf", ".woff", ".otf", ".bdf", ".pcf")


def validate_truetype_file(value):
    if value.lower().endswith(".zip"):  # for Google Fonts downloads
        raise cv.Invalid(
            f"Please unzip the font archive '{value}' first and then use the .ttf files inside."
        )
    if not any(map(value.lower().endswith, FONT_EXTENSIONS)):
        raise cv.Invalid(f"Only {', '.join(FONT_EXTENSIONS)} files are supported.")
    return CORE.relative_config_path(cv.file_(value))


def add_local_file(value: ConfigType) -> ConfigType:
    if value in FONT_CACHE:
        return value
    path = Path(value[CONF_PATH])
    if not path.is_file():
        raise cv.Invalid(f"File '{path}' not found.")
    FONT_CACHE[value] = path
    return value


TYPE_LOCAL = "local"
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


def _web_font_path(value: dict) -> Path:
    return external_files.compute_local_file_path(DOMAIN, value[CONF_URL]) / "font.ttf"


def _gfonts_css_url(value: dict) -> str:
    return (
        f"https://fonts.googleapis.com/css2?family={value[CONF_FAMILY]}"
        f":ital,wght@{int(value[CONF_ITALIC])},{value[CONF_WEIGHT]}"
    )


def _gfonts_cache_path(value: dict, suffix: str) -> Path:
    name = f"{value[CONF_FAMILY]}@{value[CONF_WEIGHT]}@{value[CONF_ITALIC]}@v1"
    return external_files.compute_local_file_dir(DOMAIN) / f"{name}.{suffix}"


def _gfonts_ttf_path(value: dict) -> Path:
    return _gfonts_cache_path(value, "ttf")


def _gfonts_css_path(value: dict) -> Path:
    return _gfonts_cache_path(value, "css")


def _parse_gfonts_css(css: str) -> str | None:
    """Extract the truetype URL from a Google Fonts CSS response."""
    match = re.search(r"src:\s+url\((.+)\)\s+format\('truetype'\);", css)
    return match.group(1) if match else None


def download_gfont(value: ConfigType) -> ConfigType:
    if value in FONT_CACHE:
        return value
    path = _gfonts_ttf_path(value)
    if not external_files.is_file_recent(path, value[CONF_REFRESH]):
        _LOGGER.debug("download_gfont: path=%s", path)
        url = _gfonts_css_url(value)
        css_path = _gfonts_css_path(value)
        try:
            css_bytes = external_files.download_content(url, css_path)
        except cv.Invalid as e:
            raise cv.Invalid(
                f"Could not download font at {url}, please check the fonts exists "
                f"at google fonts ({e})"
            ) from e
        if not (
            external_files.is_fresh_this_run(css_path) or CORE.skip_external_update
        ):
            # Same rule as PREFETCH_FILES stage two: a CSS body that could
            # not be revalidated may name a rotated ttf URL. Use the cached
            # font instead (the failed check already warned).
            if path.exists():
                FONT_CACHE[value] = path
                return value
            raise cv.Invalid(
                f"Could not refresh the Google Fonts CSS for "
                f"{value[CONF_FAMILY]} and no cached font is available"
            )
        try:
            css = css_bytes.decode("utf-8")
        except UnicodeDecodeError as e:
            # Do not leave an unusable body in the cache to be served again.
            css_path.unlink(missing_ok=True)
            raise cv.Invalid(
                f"Bad response from Google Fonts for {value[CONF_FAMILY]}: "
                f"not a text document"
            ) from e
        ttf_url = _parse_gfonts_css(css)
        if ttf_url is None:
            css_path.unlink(missing_ok=True)
            raise cv.Invalid(
                f"Could not extract ttf file from gfonts response for "
                f"{value[CONF_FAMILY]}, please report this."
            )
        _LOGGER.debug("download_gfont: ttf_url=%s", ttf_url)

        external_files.download_content(ttf_url, path)
        # In case the remote file is not modified, the download_content function will return the existing file,
        # so update the modification time to now.
        path.touch()
    FONT_CACHE[value] = path
    return value


def download_web_font(value: ConfigType) -> ConfigType:
    if value in FONT_CACHE:
        return value
    url = value[CONF_URL]
    path = _web_font_path(value)

    external_files.download_content(url, path)
    _LOGGER.debug("download_web_font: path=%s", path)
    FONT_CACHE[value] = path
    return value


# Shared by the schema and the prefetch extractor so they cannot drift.
_DEFAULT_WEIGHT = "regular"
_DEFAULT_ITALIC = False
_DEFAULT_REFRESH = "1d"
_WEIGHT_VALIDATOR = cv.Any(cv.int_, validate_weight_name)
_REFRESH_VALIDATOR = cv.All(cv.string, cv.source_refresh)

EXTERNAL_FONT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_WEIGHT, default=_DEFAULT_WEIGHT): _WEIGHT_VALIDATOR,
        cv.Optional(CONF_ITALIC, default=_DEFAULT_ITALIC): cv.boolean,
        cv.Optional(CONF_REFRESH, default=_DEFAULT_REFRESH): _REFRESH_VALIDATOR,
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


_GFONTS_SHORTHAND_RE = re.compile(r"^gfonts://([^@]+)(@.+)?$")


def _shorthand_to_file_dict(value: str) -> ConfigType | None:
    """Typed-dict form of a remote font shorthand.

    Shared by the schema validator and the prefetch extractor so the two
    cannot drift. Returns None for values that are not remote shorthand
    (i.e. local paths); raises cv.Invalid for a malformed gfonts shorthand.
    """
    if value.startswith("gfonts://"):
        if (match := _GFONTS_SHORTHAND_RE.match(value)) is None:
            raise cv.Invalid("Could not parse gfonts shorthand syntax, please check it")
        data = {CONF_TYPE: TYPE_GFONTS, CONF_FAMILY: match.group(1)}
        if match.group(2):
            data[CONF_WEIGHT] = match.group(2)[1:]
        return data
    if value.startswith(("http://", "https://")):
        return {CONF_TYPE: TYPE_WEB, CONF_URL: value}
    return None


def _extract_remote_font(value: object) -> ConfigType | None:
    """Map a raw, pre-schema font `file:` value to a normalized remote spec.

    Read-only mirror of `validate_file_shorthand` / `TYPED_FILE_SCHEMA` for
    the prefetch hooks; returns None for local fonts and anything it does
    not recognize. A wrong answer only wastes or misses a prefetch, the
    schema validators stay authoritative.
    """
    if isinstance(value, str):
        try:
            value = _shorthand_to_file_dict(value)
        except cv.Invalid:
            return None
    if not isinstance(value, dict):
        return None
    font_type = value.get(CONF_TYPE)
    if font_type == TYPE_WEB and isinstance(url := value.get(CONF_URL), str):
        return {CONF_TYPE: TYPE_WEB, CONF_URL: url}
    if font_type == TYPE_GFONTS and isinstance(family := value.get(CONF_FAMILY), str):
        try:
            italic = cv.boolean(value.get(CONF_ITALIC, _DEFAULT_ITALIC))
            weight = _WEIGHT_VALIDATOR(value.get(CONF_WEIGHT, _DEFAULT_WEIGHT))
            refresh = _REFRESH_VALIDATOR(value.get(CONF_REFRESH, _DEFAULT_REFRESH))
        except cv.Invalid:
            return None
        return {
            CONF_TYPE: TYPE_GFONTS,
            CONF_FAMILY: family,
            CONF_WEIGHT: weight,
            CONF_ITALIC: italic,
            CONF_REFRESH: refresh,
        }
    return None


def _iter_remote_specs(entries: list[ConfigType]) -> Iterable[ConfigType]:
    """Yield the remote spec of every `file:` value, including extras."""
    for entry in entries:
        values = [entry.get(CONF_FILE)]
        extras = entry.get(CONF_EXTRAS)
        if isinstance(extras, dict):
            # The schema runs cv.ensure_list on extras, so a bare mapping
            # is valid raw config; mirror that normalization here.
            extras = [extras]
        if isinstance(extras, list):
            values.extend(
                extra.get(CONF_FILE) for extra in extras if isinstance(extra, dict)
            )
        for value in values:
            if (spec := _extract_remote_font(value)) is not None:
                yield spec


def PREFETCH_FILES(entries: list[ConfigType]) -> Iterable[list[RemoteFile]]:
    """Batch-download hook: web fonts, then Google Fonts CSS, then ttf.

    Stage one fetches web fonts and the CSS of stale gfonts; stage two
    parses the now-cached CSS for the ttf URLs it names.
    """
    stage1: list[RemoteFile] = []
    # Keyed by cache path: the same font at several sizes is one download,
    # one freshness stat, and one stage-two CSS parse.
    stale_gfonts: dict[Path, ConfigType] = {}
    seen_web: set[Path] = set()
    for spec in _iter_remote_specs(entries):
        if spec[CONF_TYPE] == TYPE_WEB:
            if (path := _web_font_path(spec)) not in seen_web:
                seen_web.add(path)
                stage1.append(RemoteFile(spec[CONF_URL], path))
        elif (css_path := _gfonts_css_path(spec)) not in stale_gfonts and (
            not external_files.is_file_recent(
                _gfonts_ttf_path(spec), spec[CONF_REFRESH]
            )
        ):
            stale_gfonts[css_path] = spec
            stage1.append(RemoteFile(_gfonts_css_url(spec), css_path))
    yield stage1

    yield [
        RemoteFile(ttf_url, _gfonts_ttf_path(spec))
        for css_path, spec in stale_gfonts.items()
        # Only trust CSS that stage one actually refreshed this run; a
        # leftover from an earlier run may name a rotated ttf URL.
        if external_files.is_fresh_this_run(css_path)
        and css_path.exists()
        and (ttf_url := _parse_gfonts_css(css_path.read_text("utf-8", "replace")))
        is not None
    ]


def validate_file_shorthand(value: object) -> ConfigType:
    value = cv.string_strict(value)
    if (data := _shorthand_to_file_dict(value)) is None:
        data = {CONF_TYPE: TYPE_LOCAL, CONF_PATH: value}
    return font_file_schema(data)


TYPED_FILE_SCHEMA = cv.typed_schema(
    {
        TYPE_LOCAL: LOCAL_SCHEMA,
        TYPE_GFONTS: GFONTS_SCHEMA,
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
        cv.GenerateID(CONF_RAW_GLYPH_ID): cv.declare_id(Glyph),
    },
)

CONFIG_SCHEMA = cv.All(FONT_SCHEMA, validate_font_config)


class EFont:
    def __init__(self, file, codepoints):
        self.codepoints = codepoints
        self.font: Face = FONT_CACHE[file]


class GlyphInfo:
    def __init__(self, glyph, data, advance, offset_x, offset_y, width, height):
        self.glyph = glyph
        self.bitmap_data = data
        self.advance = advance
        self.offset_x = offset_x
        self.offset_y = offset_y
        self.width = width
        self.height = height


def glyph_to_glyphinfo(glyph, font, size, bpp):
    # Convert to 32 bit unicode codepoint
    glyph = ord(glyph)
    scale = 256 // (1 << bpp)
    if not font.is_scalable:
        sizes = [pt_to_px(x.size) for x in font.available_sizes]
        if size in sizes:
            font.select_size(sizes.index(size))
    else:
        font.set_pixel_sizes(size, 0)
    flags = FT_LOAD_RENDER
    if bpp != 1:
        flags |= FT_LOAD_NO_BITMAP
    else:
        flags |= FT_LOAD_TARGET_MONO
    font.load_char(glyph, flags)
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
    ascender = pt_to_px(font.size.ascender)
    if ascender == 0:
        if not font.is_scalable:
            ascender = size
        else:
            _LOGGER.error(
                "Unable to determine ascender of font %s %s",
                font.family_name,
                font.style_name,
            )
    return GlyphInfo(
        glyph,
        glyph_data,
        pt_to_px(font.glyph.metrics.horiAdvance),
        font.glyph.bitmap_left,
        ascender - font.glyph.bitmap_top,
        width,
        height,
    )


async def to_code(config):
    """
    Collect all glyph codepoints, construct a map from a codepoint to a font file.
    Codepoints are either explicit (glyphs key in top level or extras) or part of a glyphset.
    Codepoints listed in extras use the extra font and override codepoints from glyphsets.
    Achieve this by processing the base codepoints first, then the extras
    """

    # get the codepoints from glyphsets and flatten to a set of chrs.
    cg.add_define("USE_FONT")
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
    point_font_map: dict[str, Face] = dict.fromkeys(point_set, base_font)
    # process extras, updating the map and extending the codepoint list
    for extra in config[CONF_EXTRAS]:
        extra_points = flatten(extra[CONF_GLYPHS])
        point_set.update(extra_points)
        extra_font = FONT_CACHE[extra[CONF_FILE]]
        point_font_map.update(dict.fromkeys(extra_points, extra_font))

    codepoints = list(point_set)
    codepoints.sort(key=functools.cmp_to_key(glyph_comparator))
    bpp = config[CONF_BPP]
    size = config[CONF_SIZE]
    # create the data array for all glyphs
    glyph_args = [
        glyph_to_glyphinfo(x, point_font_map[x], size, bpp) for x in codepoints
    ]
    rhs = [HexInt(x) for x in flatten([x.bitmap_data for x in glyph_args])]
    prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)

    # Create the glyph table that points to data in the above array.
    glyph_initializer = [
        [
            x.glyph,
            prog_arr + (y - len(x.bitmap_data)),
            x.advance,
            x.offset_x,
            x.offset_y,
            x.width,
            x.height,
        ]
        for (x, y) in zip(
            glyph_args,
            list(accumulate([len(x.bitmap_data) for x in glyph_args])),
            strict=True,
        )
    ]

    glyphs = cg.static_const_array(config[CONF_RAW_GLYPH_ID], glyph_initializer)

    font_height = pt_to_px(base_font.size.height)
    ascender = pt_to_px(base_font.size.ascender)
    descender = abs(pt_to_px(base_font.size.descender))
    g = glyph_to_glyphinfo("x", base_font, size, bpp)
    xheight = g.height if len(g.bitmap_data) > 1 else 0
    g = glyph_to_glyphinfo("X", base_font, size, bpp)
    capheight = g.height if len(g.bitmap_data) > 1 else 0
    if font_height == 0:
        if not base_font.is_scalable:
            font_height = size
            ascender = font_height
        else:
            _LOGGER.error("Unable to determine height of font %s", config[CONF_FILE])
    cg.new_Pvariable(
        config[CONF_ID],
        glyphs,
        len(glyph_initializer),
        ascender,
        font_height,
        descender,
        xheight,
        capheight,
        bpp,
    )
