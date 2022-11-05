import logging

from esphome import core
from esphome.components import display, font
import esphome.components.image as espImage
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_FILE, CONF_ID, CONF_RAW_DATA_ID, CONF_RESIZE, CONF_TYPE
from esphome.core import CORE, HexInt

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["display"]
MULTI_CONF = True

Animation_ = display.display_ns.class_("Animation", espImage.Image_)

ANIMATION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(Animation_),
        cv.Required(CONF_FILE): cv.file_,
        cv.Optional(CONF_RESIZE): cv.dimensions,
        cv.Optional(CONF_TYPE, default="BINARY"): cv.enum(
            espImage.IMAGE_TYPE, upper=True
        ),
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
    }
)

CONFIG_SCHEMA = cv.All(font.validate_pillow_installed, ANIMATION_SCHEMA)

CODEOWNERS = ["@syndlex"]


async def to_code(config):
    from PIL import Image

    path = CORE.relative_config_path(config[CONF_FILE])
    try:
        image = Image.open(path)
    except Exception as e:
        raise core.EsphomeError(f"Could not load image file {path}: {e}")

    width, height = image.size
    frames = image.n_frames
    if CONF_RESIZE in config:
        new_width_max, new_height_max = config[CONF_RESIZE]
        ratio = min(new_width_max / width, new_height_max / height)
        width, height = int(width * ratio), int(height * ratio)
    else:
        if width > 500 or height > 500:
            _LOGGER.warning(
                "The image you requested is very big. Please consider using"
                " the resize parameter."
            )

    if config[CONF_TYPE] == "GRAYSCALE":
        data = [0 for _ in range(height * width * frames)]
        pos = 0
        for frameIndex in range(frames):
            image.seek(frameIndex)
            frame = image.convert("L", dither=Image.NONE)
            if CONF_RESIZE in config:
                frame = frame.resize([width, height])
            pixels = list(frame.getdata())
            if len(pixels) != height * width:
                raise core.EsphomeError(
                    f"Unexpected number of pixels in {path} frame {frameIndex}: ({len(pixels)} != {height*width})"
                )
            for pix in pixels:
                data[pos] = pix
                pos += 1

    elif config[CONF_TYPE] == "RGB24":
        data = [0 for _ in range(height * width * 3 * frames)]
        pos = 0
        for frameIndex in range(frames):
            image.seek(frameIndex)
            if CONF_RESIZE in config:
                image.thumbnail(config[CONF_RESIZE])
            frame = image.convert("RGB")
            if CONF_RESIZE in config:
                frame = frame.resize([width, height])
            pixels = list(frame.getdata())
            if len(pixels) != height * width:
                raise core.EsphomeError(
                    f"Unexpected number of pixels in {path} frame {frameIndex}: ({len(pixels)} != {height*width})"
                )
            for pix in pixels:
                data[pos] = pix[0]
                pos += 1
                data[pos] = pix[1]
                pos += 1
                data[pos] = pix[2]
                pos += 1

    elif config[CONF_TYPE] == "RGB565":
        data = [0 for _ in range(height * width * 2 * frames)]
        pos = 0
        for frameIndex in range(frames):
            image.seek(frameIndex)
            frame = image.convert("RGB")
            if CONF_RESIZE in config:
                frame = frame.resize([width, height])
            pixels = list(frame.getdata())
            if len(pixels) != height * width:
                raise core.EsphomeError(
                    f"Unexpected number of pixels in {path} frame {frameIndex}: ({len(pixels)} != {height*width})"
                )
            for pix in pixels:
                R = pix[0] >> 3
                G = pix[1] >> 2
                B = pix[2] >> 3
                rgb = (R << 11) | (G << 5) | B
                data[pos] = rgb >> 8
                pos += 1
                data[pos] = rgb & 255
                pos += 1

    elif config[CONF_TYPE] == "BINARY":
        width8 = ((width + 7) // 8) * 8
        data = [0 for _ in range((height * width8 // 8) * frames)]
        for frameIndex in range(frames):
            image.seek(frameIndex)
            frame = image.convert("1", dither=Image.NONE)
            if CONF_RESIZE in config:
                frame = frame.resize([width, height])
            for y in range(height):
                for x in range(width):
                    if frame.getpixel((x, y)):
                        continue
                    pos = x + y * width8 + (height * width8 * frameIndex)
                    data[pos // 8] |= 0x80 >> (pos % 8)

    rhs = [HexInt(x) for x in data]
    prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
    cg.new_Pvariable(
        config[CONF_ID],
        prog_arr,
        width,
        height,
        frames,
        espImage.IMAGE_TYPE[config[CONF_TYPE]],
    )
