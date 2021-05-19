import logging

from esphome import core
from esphome.components import display, font
import esphome.components.image as espImage
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_FILE, CONF_ID, CONF_TYPE, CONF_RESIZE
from esphome.core import CORE, HexInt

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["display"]
MULTI_CONF = True

Animation_ = display.display_ns.class_("Animation")

CONF_RAW_DATA_ID = "raw_data_id"

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


def to_code(config):
    from PIL import Image

    path = CORE.relative_config_path(config[CONF_FILE])
    try:
        image = Image.open(path)
    except Exception as e:
        raise core.EsphomeError(f"Could not load image file {path}: {e}")

    width, height = image.size
    frames = image.n_frames
    if CONF_RESIZE in config:
        image.thumbnail(config[CONF_RESIZE])
        width, height = image.size
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
            pixels = list(frame.getdata())
            for pix in pixels:
                data[pos] = pix
                pos += 1

    elif config[CONF_TYPE] == "RGB24":
        data = [0 for _ in range(height * width * 3 * frames)]
        pos = 0
        for frameIndex in range(frames):
            image.seek(frameIndex)
            frame = image.convert("RGB")
            pixels = list(frame.getdata())
            for pix in pixels:
                data[pos] = pix[0]
                pos += 1
                data[pos] = pix[1]
                pos += 1
                data[pos] = pix[2]
                pos += 1

    elif config[CONF_TYPE] == "BINARY":
        width8 = ((width + 7) // 8) * 8
        data = [0 for _ in range((height * width8 // 8) * frames)]
        for frameIndex in range(frames):
            image.seek(frameIndex)
            frame = image.convert("1", dither=Image.NONE)
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
