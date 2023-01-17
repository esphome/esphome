import logging

from esphome import core
from esphome.components import display, font
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_DITHER,
    CONF_FILE,
    CONF_ID,
    CONF_RAW_DATA_ID,
    CONF_RESIZE,
    CONF_TYPE,
)
from esphome.core import CORE, HexInt

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["display"]
MULTI_CONF = True

ImageType = display.display_ns.enum("ImageType")
IMAGE_TYPE = {
    "BINARY": ImageType.IMAGE_TYPE_BINARY,
    "GRAYSCALE": ImageType.IMAGE_TYPE_GRAYSCALE,
    "RGB24": ImageType.IMAGE_TYPE_RGB24,
    "TRANSPARENT_BINARY": ImageType.IMAGE_TYPE_BINARY,
    "RGB565": ImageType.IMAGE_TYPE_RGB565,
    "RGB332": ImageType.IMAGE_TYPE_RGB323,
    "TRANSPARENT_IMAGE": ImageType.IMAGE_TYPE_RGB565,
}

Image_ = display.display_ns.class_("Image")

IMAGE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(Image_),
        cv.Required(CONF_FILE): cv.file_,
        cv.Optional(CONF_RESIZE): cv.dimensions,
        cv.Optional(CONF_TYPE, default="BINARY"): cv.enum(IMAGE_TYPE, upper=True),
        cv.Optional(CONF_DITHER, default="NONE"): cv.one_of(
            "NONE", "FLOYDSTEINBERG", upper=True
        ),
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
    }
)

CONFIG_SCHEMA = cv.All(font.validate_pillow_installed, IMAGE_SCHEMA)


async def to_code(config):
    from PIL import Image

    path = CORE.relative_config_path(config[CONF_FILE])
    try:
        image = Image.open(path)
    except Exception as e:
        raise core.EsphomeError(f"Could not load image file {path}: {e}")

    width, height = image.size

    if CONF_RESIZE in config:
        image.thumbnail(config[CONF_RESIZE])
        width, height = image.size
    else:
        if width > 500 or height > 500:
            _LOGGER.warning(
                "The image you requested is very big. Please consider using"
                " the resize parameter."
            )

    dither = Image.NONE if config[CONF_DITHER] == "NONE" else Image.FLOYDSTEINBERG

    if config[CONF_TYPE] == "GRAYSCALE":
        image = image.convert("LA", dither=dither)
        pixels = list(image.getdata())
        data = [0 for _ in range(height * width)]
        pos = 0
        for pix in pixels:
            if pix[0] == 1:
                pix[0] = 0
            if pix[1] > 127:
                pix[0] = 1

            data[pos] = pix
            pos += 1

    elif config[CONF_TYPE] == "RGB24":
        image = image.convert("RGBA")
        pixels = list(image.getdata())
        data = [0 for _ in range(height * width * 3)]
        pos = 0
        for pix in pixels:
            if pix[0] == 0 and pix[1] == 0 and pix[2] == 1:
                pix[2] = 0
            if pix[3] > 127:
                pix[0] = 0
                pix[1] = 0
                pix[2] = 1

            data[pos] = pix[0]
            pos += 1
            data[pos] = pix[1]
            pos += 1
            data[pos] = pix[2]
            pos += 1

    elif config[CONF_TYPE] in ["RGB565", "TRANSPARENT_IMAGE"]:
        image = image.convert("RGBA")
        pixels = list(image.getdata())
        data = [0 for _ in range(height * width * 1)]
        pos = 0
        for pix in pixels:
            R = pix[0] >> 3
            G = pix[1] >> 2
            B = pix[2] >> 3
            rgb = (R << 11) | (G << 5) | B

            if rgb == 1:
                rgb = 0
            if pix[3] > 127:
                rgb = 1

            data[pos] = rgb >> 8
            pos += 1
            data[pos] = rgb & 255
            pos += 1

    elif config[CONF_TYPE] in ["BINARY", "TRANSPARENT_BINARY"]:
        image = image.convert("1", dither=dither)
        width8 = ((width + 7) // 8) * 8
        data = [0 for _ in range(height * width8 // 8)]
        for y in range(height):
            for x in range(width):
                if image.getpixel((x, y)):
                    continue
                pos = x + y * width8
                data[pos // 8] |= 0x80 >> (pos % 8)
    else:
        raise core.EsphomeError(f"Image type {config[CONF_TYPE]} is not supported.")

    rhs = [HexInt(x) for x in data]
    prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
    cg.new_Pvariable(
        config[CONF_ID], prog_arr, width, height, IMAGE_TYPE[config[CONF_TYPE]]
    )
