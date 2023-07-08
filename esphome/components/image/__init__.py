import logging

import io
from pathlib import Path
import re
import requests

from esphome import core
from esphome.components import font
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_DITHER,
    CONF_FILE,
    CONF_ICON,
    CONF_ID,
    CONF_PATH,
    CONF_RAW_DATA_ID,
    CONF_RESIZE,
    CONF_SOURCE,
    CONF_TYPE,
)
from esphome.core import CORE, HexInt

_LOGGER = logging.getLogger(__name__)

DOMAIN = "image"
DEPENDENCIES = ["display"]
MULTI_CONF = True

image_ns = cg.esphome_ns.namespace("image")

ImageType = image_ns.enum("ImageType")
IMAGE_TYPE = {
    "BINARY": ImageType.IMAGE_TYPE_BINARY,
    "TRANSPARENT_BINARY": ImageType.IMAGE_TYPE_BINARY,
    "GRAYSCALE": ImageType.IMAGE_TYPE_GRAYSCALE,
    "RGB565": ImageType.IMAGE_TYPE_RGB565,
    "RGB24": ImageType.IMAGE_TYPE_RGB24,
    "RGBA": ImageType.IMAGE_TYPE_RGBA,
}

CONF_USE_TRANSPARENCY = "use_transparency"

# If the MDI file cannot be downloaded within this time, abort.
MDI_DOWNLOAD_TIMEOUT = 30  # seconds

SOURCE_LOCAL = "local"
SOURCE_MDI = "mdi"

Image_ = image_ns.class_("Image")


def _compute_local_icon_path(value) -> Path:
    base_dir = Path(CORE.config_dir) / ".esphome" / DOMAIN / "mdi"
    return base_dir / f"{value[CONF_ICON]}.svg"


def download_mdi(value):
    mdi_id = value[CONF_ICON]
    path = _compute_local_icon_path(value)
    if path.is_file():
        return value
    url = f"https://raw.githubusercontent.com/Templarian/MaterialDesign/master/svg/{mdi_id}.svg"
    _LOGGER.debug("Downloading %s MDI image from %s", mdi_id, url)
    try:
        req = requests.get(url, timeout=MDI_DOWNLOAD_TIMEOUT)
        req.raise_for_status()
    except requests.exceptions.RequestException as e:
        raise cv.Invalid(f"Could not download MDI image {mdi_id} from {url}: {e}")

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(req.content)
    return value


def validate_cairosvg_installed(value):
    """Validate that cairosvg is installed"""
    try:
        import cairosvg
    except ImportError as err:
        raise cv.Invalid(
            "Please install the cairosvg python package to use this feature. "
            "(pip install cairosvg)"
        ) from err

    major, minor, _ = cairosvg.__version__.split(".")
    if major < "2" or major == "2" and minor < "2":
        raise cv.Invalid(
            "Please update your cairosvg installation to at least 2.2.0. "
            "(pip install -U cairosvg)"
        )

    return value


def validate_cross_dependencies(config):
    """
    Validate fields whose possible values depend on other fields.
    For example, validate that explicitly transparent image types
    have "use_transparency" set to True.
    Also set the default value for those kind of dependent fields.
    """
    is_mdi = CONF_FILE in config and config[CONF_FILE][CONF_SOURCE] == SOURCE_MDI
    if CONF_TYPE not in config:
        if is_mdi:
            config[CONF_TYPE] = "TRANSPARENT_BINARY"
        else:
            config[CONF_TYPE] = "BINARY"

    image_type = config[CONF_TYPE]
    is_transparent_type = image_type in ["TRANSPARENT_BINARY", "RGBA"]

    # If the use_transparency option was not specified, set the default depending on the image type
    if CONF_USE_TRANSPARENCY not in config:
        config[CONF_USE_TRANSPARENCY] = is_transparent_type

    if is_transparent_type and not config[CONF_USE_TRANSPARENCY]:
        raise cv.Invalid(f"Image type {image_type} must always be transparent.")

    if is_mdi and config[CONF_TYPE] not in ["BINARY", "TRANSPARENT_BINARY"]:
        raise cv.Invalid("MDI images must be binary images.")

    return config


def validate_file_shorthand(value):
    value = cv.string_strict(value)
    if value.startswith("mdi:"):
        validate_cairosvg_installed(value)

        match = re.search(r"mdi:([a-zA-Z0-9\-]+)", value)
        if match is None:
            raise cv.Invalid("Could not parse mdi icon name.")
        icon = match.group(1)
        return FILE_SCHEMA(
            {
                CONF_SOURCE: SOURCE_MDI,
                CONF_ICON: icon,
            }
        )
    return FILE_SCHEMA(
        {
            CONF_SOURCE: SOURCE_LOCAL,
            CONF_PATH: value,
        }
    )


LOCAL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PATH): cv.file_,
    }
)

MDI_SCHEMA = cv.All(
    {
        cv.Required(CONF_ICON): cv.string,
    },
    download_mdi,
)

TYPED_FILE_SCHEMA = cv.typed_schema(
    {
        SOURCE_LOCAL: LOCAL_SCHEMA,
        SOURCE_MDI: MDI_SCHEMA,
    },
    key=CONF_SOURCE,
)


def _file_schema(value):
    if isinstance(value, str):
        return validate_file_shorthand(value)
    return TYPED_FILE_SCHEMA(value)


FILE_SCHEMA = cv.Schema(_file_schema)

IMAGE_SCHEMA = cv.Schema(
    cv.All(
        {
            cv.Required(CONF_ID): cv.declare_id(Image_),
            cv.Required(CONF_FILE): FILE_SCHEMA,
            cv.Optional(CONF_RESIZE): cv.dimensions,
            # Not setting default here on purpose; the default depends on the source type
            # (file or mdi), and will be set in the "validate_cross_dependencies" validator.
            cv.Optional(CONF_TYPE): cv.enum(IMAGE_TYPE, upper=True),
            # Not setting default here on purpose; the default depends on the image type,
            # and thus will be set in the "validate_cross_dependencies" validator.
            cv.Optional(CONF_USE_TRANSPARENCY): cv.boolean,
            cv.Optional(CONF_DITHER, default="NONE"): cv.one_of(
                "NONE", "FLOYDSTEINBERG", upper=True
            ),
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        },
        validate_cross_dependencies,
    )
)

CONFIG_SCHEMA = cv.All(font.validate_pillow_installed, IMAGE_SCHEMA)


def load_svg_image(file: str, resize: tuple[int, int]):
    from PIL import Image

    # This import is only needed in case of SVG images; adding it
    # to the top would force configurations not using SVG to also have it
    # installed for no reason.
    from cairosvg import svg2png

    if resize:
        req_width, req_height = resize
        svg_image = svg2png(
            url=file,
            output_width=req_width,
            output_height=req_height,
        )
    else:
        svg_image = svg2png(url=file)

    return Image.open(io.BytesIO(svg_image))


async def to_code(config):
    from PIL import Image

    conf_file = config[CONF_FILE]

    if conf_file[CONF_SOURCE] == SOURCE_LOCAL:
        path = CORE.relative_config_path(conf_file[CONF_PATH])

    elif conf_file[CONF_SOURCE] == SOURCE_MDI:
        path = _compute_local_icon_path(conf_file).as_posix()

    try:
        resize = config.get(CONF_RESIZE)
        if path.lower().endswith(".svg"):
            image = load_svg_image(path, resize)
        else:
            image = Image.open(path)
            if resize:
                image.thumbnail(resize)
    except Exception as e:
        raise core.EsphomeError(f"Could not load image file {path}: {e}")

    width, height = image.size

    if CONF_RESIZE not in config and (width > 500 or height > 500):
        _LOGGER.warning(
            'The image "%s" you requested is very big. Please consider'
            " using the resize parameter.",
            path,
        )

    transparent = config[CONF_USE_TRANSPARENCY]

    dither = Image.NONE if config[CONF_DITHER] == "NONE" else Image.FLOYDSTEINBERG
    if config[CONF_TYPE] == "GRAYSCALE":
        image = image.convert("LA", dither=dither)
        pixels = list(image.getdata())
        data = [0 for _ in range(height * width)]
        pos = 0
        for g, a in pixels:
            if transparent:
                if g == 1:
                    g = 0
                if a < 0x80:
                    g = 1

            data[pos] = g
            pos += 1

    elif config[CONF_TYPE] == "RGBA":
        image = image.convert("RGBA")
        pixels = list(image.getdata())
        data = [0 for _ in range(height * width * 4)]
        pos = 0
        for r, g, b, a in pixels:
            data[pos] = r
            pos += 1
            data[pos] = g
            pos += 1
            data[pos] = b
            pos += 1
            data[pos] = a
            pos += 1

    elif config[CONF_TYPE] == "RGB24":
        image = image.convert("RGBA")
        pixels = list(image.getdata())
        data = [0 for _ in range(height * width * 3)]
        pos = 0
        for r, g, b, a in pixels:
            if transparent:
                if r == 0 and g == 0 and b == 1:
                    b = 0
                if a < 0x80:
                    r = 0
                    g = 0
                    b = 1

            data[pos] = r
            pos += 1
            data[pos] = g
            pos += 1
            data[pos] = b
            pos += 1

    elif config[CONF_TYPE] in ["RGB565"]:
        image = image.convert("RGBA")
        pixels = list(image.getdata())
        data = [0 for _ in range(height * width * 2)]
        pos = 0
        for r, g, b, a in pixels:
            R = r >> 3
            G = g >> 2
            B = b >> 3
            rgb = (R << 11) | (G << 5) | B

            if transparent:
                if rgb == 0x0020:
                    rgb = 0
                if a < 0x80:
                    rgb = 0x0020

            data[pos] = rgb >> 8
            pos += 1
            data[pos] = rgb & 0xFF
            pos += 1

    elif config[CONF_TYPE] in ["BINARY", "TRANSPARENT_BINARY"]:
        if transparent:
            alpha = image.split()[-1]
            has_alpha = alpha.getextrema()[0] < 0xFF
            _LOGGER.debug("%s Has alpha: %s", config[CONF_ID], has_alpha)
        image = image.convert("1", dither=dither)
        width8 = ((width + 7) // 8) * 8
        data = [0 for _ in range(height * width8 // 8)]
        for y in range(height):
            for x in range(width):
                if transparent and has_alpha:
                    a = alpha.getpixel((x, y))
                    if not a:
                        continue
                elif image.getpixel((x, y)):
                    continue
                pos = x + y * width8
                data[pos // 8] |= 0x80 >> (pos % 8)
    else:
        raise core.EsphomeError(
            f"Image f{config[CONF_ID]} has an unsupported type: {config[CONF_TYPE]}."
        )

    rhs = [HexInt(x) for x in data]
    prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
    var = cg.new_Pvariable(
        config[CONF_ID], prog_arr, width, height, IMAGE_TYPE[config[CONF_TYPE]]
    )
    cg.add(var.set_transparency(transparent))
