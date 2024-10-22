import logging

from esphome import automation, core
import esphome.codegen as cg
from esphome.components import font
import esphome.components.image as espImage
from esphome.components.image import (
    CONF_USE_TRANSPARENCY,
    LOCAL_SCHEMA,
    SOURCE_LOCAL,
    SOURCE_WEB,
    WEB_SCHEMA,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_FILE,
    CONF_ID,
    CONF_PATH,
    CONF_RAW_DATA_ID,
    CONF_REPEAT,
    CONF_RESIZE,
    CONF_SOURCE,
    CONF_TYPE,
    CONF_URL,
)
from esphome.core import CORE, HexInt

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["image"]
CODEOWNERS = ["@syndlex"]
DEPENDENCIES = ["display"]
MULTI_CONF = True

CONF_LOOP = "loop"
CONF_START_FRAME = "start_frame"
CONF_END_FRAME = "end_frame"
CONF_FRAME = "frame"

animation_ns = cg.esphome_ns.namespace("animation")

Animation_ = animation_ns.class_("Animation", espImage.Image_)

# Actions
NextFrameAction = animation_ns.class_(
    "AnimationNextFrameAction", automation.Action, cg.Parented.template(Animation_)
)
PrevFrameAction = animation_ns.class_(
    "AnimationPrevFrameAction", automation.Action, cg.Parented.template(Animation_)
)
SetFrameAction = animation_ns.class_(
    "AnimationSetFrameAction", automation.Action, cg.Parented.template(Animation_)
)

TYPED_FILE_SCHEMA = cv.typed_schema(
    {
        SOURCE_LOCAL: LOCAL_SCHEMA,
        SOURCE_WEB: WEB_SCHEMA,
    },
    key=CONF_SOURCE,
)


def _file_schema(value):
    if isinstance(value, str):
        return validate_file_shorthand(value)
    return TYPED_FILE_SCHEMA(value)


FILE_SCHEMA = cv.Schema(_file_schema)


def validate_file_shorthand(value):
    value = cv.string_strict(value)
    if value.startswith("http://") or value.startswith("https://"):
        return FILE_SCHEMA(
            {
                CONF_SOURCE: SOURCE_WEB,
                CONF_URL: value,
            }
        )
    return FILE_SCHEMA(
        {
            CONF_SOURCE: SOURCE_LOCAL,
            CONF_PATH: value,
        }
    )


def validate_cross_dependencies(config):
    """
    Validate fields whose possible values depend on other fields.
    For example, validate that explicitly transparent image types
    have "use_transparency" set to True.
    Also set the default value for those kind of dependent fields.
    """
    image_type = config[CONF_TYPE]
    is_transparent_type = image_type in ["TRANSPARENT_BINARY", "RGBA"]
    # If the use_transparency option was not specified, set the default depending on the image type
    if CONF_USE_TRANSPARENCY not in config:
        config[CONF_USE_TRANSPARENCY] = is_transparent_type

    if is_transparent_type and not config[CONF_USE_TRANSPARENCY]:
        raise cv.Invalid(f"Image type {image_type} must always be transparent.")

    return config


ANIMATION_SCHEMA = cv.Schema(
    cv.All(
        {
            cv.Required(CONF_ID): cv.declare_id(Animation_),
            cv.Required(CONF_FILE): FILE_SCHEMA,
            cv.Optional(CONF_RESIZE): cv.dimensions,
            cv.Optional(CONF_TYPE, default="BINARY"): cv.enum(
                espImage.IMAGE_TYPE, upper=True
            ),
            # Not setting default here on purpose; the default depends on the image type,
            # and thus will be set in the "validate_cross_dependencies" validator.
            cv.Optional(CONF_USE_TRANSPARENCY): cv.boolean,
            cv.Optional(CONF_LOOP): cv.All(
                {
                    cv.Optional(CONF_START_FRAME, default=0): cv.positive_int,
                    cv.Optional(CONF_END_FRAME): cv.positive_int,
                    cv.Optional(CONF_REPEAT): cv.positive_int,
                }
            ),
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
        },
        validate_cross_dependencies,
    )
)

CONFIG_SCHEMA = cv.All(font.validate_pillow_installed, ANIMATION_SCHEMA)

NEXT_FRAME_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(Animation_),
    }
)
PREV_FRAME_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(Animation_),
    }
)
SET_FRAME_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(Animation_),
        cv.Required(CONF_FRAME): cv.uint16_t,
    }
)


@automation.register_action("animation.next_frame", NextFrameAction, NEXT_FRAME_SCHEMA)
@automation.register_action("animation.prev_frame", PrevFrameAction, PREV_FRAME_SCHEMA)
@automation.register_action("animation.set_frame", SetFrameAction, SET_FRAME_SCHEMA)
async def animation_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    if (frame := config.get(CONF_FRAME)) is not None:
        template_ = await cg.templatable(frame, args, cg.uint16)
        cg.add(var.set_frame(template_))
    return var


async def to_code(config):
    from PIL import Image

    conf_file = config[CONF_FILE]
    if conf_file[CONF_SOURCE] == SOURCE_LOCAL:
        path = CORE.relative_config_path(conf_file[CONF_PATH])
    elif conf_file[CONF_SOURCE] == SOURCE_WEB:
        path = espImage.compute_local_image_path(conf_file).as_posix()
    else:
        raise core.EsphomeError(f"Unknown animation source: {conf_file[CONF_SOURCE]}")

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
    elif width > 500 or height > 500:
        _LOGGER.warning(
            'The image "%s" you requested is very big. Please consider'
            " using the resize parameter.",
            path,
        )

    transparent = config[CONF_USE_TRANSPARENCY]

    if config[CONF_TYPE] == "GRAYSCALE":
        data = [0 for _ in range(height * width * frames)]
        pos = 0
        for frameIndex in range(frames):
            image.seek(frameIndex)
            frame = image.convert("LA", dither=Image.Dither.NONE)
            if CONF_RESIZE in config:
                frame = frame.resize([width, height])
            pixels = list(frame.getdata())
            if len(pixels) != height * width:
                raise core.EsphomeError(
                    f"Unexpected number of pixels in {path} frame {frameIndex}: ({len(pixels)} != {height * width})"
                )
            for pix, a in pixels:
                if transparent:
                    if pix == 1:
                        pix = 0
                    if a < 0x80:
                        pix = 1

                data[pos] = pix
                pos += 1

    elif config[CONF_TYPE] == "RGBA":
        data = [0 for _ in range(height * width * 4 * frames)]
        pos = 0
        for frameIndex in range(frames):
            image.seek(frameIndex)
            frame = image.convert("RGBA")
            if CONF_RESIZE in config:
                frame = frame.resize([width, height])
            pixels = list(frame.getdata())
            if len(pixels) != height * width:
                raise core.EsphomeError(
                    f"Unexpected number of pixels in {path} frame {frameIndex}: ({len(pixels)} != {height * width})"
                )
            for pix in pixels:
                data[pos] = pix[0]
                pos += 1
                data[pos] = pix[1]
                pos += 1
                data[pos] = pix[2]
                pos += 1
                data[pos] = pix[3]
                pos += 1

    elif config[CONF_TYPE] == "RGB24":
        data = [0 for _ in range(height * width * 3 * frames)]
        pos = 0
        for frameIndex in range(frames):
            image.seek(frameIndex)
            frame = image.convert("RGBA")
            if CONF_RESIZE in config:
                frame = frame.resize([width, height])
            pixels = list(frame.getdata())
            if len(pixels) != height * width:
                raise core.EsphomeError(
                    f"Unexpected number of pixels in {path} frame {frameIndex}: ({len(pixels)} != {height * width})"
                )
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

    elif config[CONF_TYPE] in ["RGB565", "TRANSPARENT_IMAGE"]:
        bytes_per_pixel = 3 if transparent else 2
        data = [0 for _ in range(height * width * bytes_per_pixel * frames)]
        pos = 0
        for frameIndex in range(frames):
            image.seek(frameIndex)
            frame = image.convert("RGBA")
            if CONF_RESIZE in config:
                frame = frame.resize([width, height])
            pixels = list(frame.getdata())
            if len(pixels) != height * width:
                raise core.EsphomeError(
                    f"Unexpected number of pixels in {path} frame {frameIndex}: ({len(pixels)} != {height * width})"
                )
            for r, g, b, a in pixels:
                R = r >> 3
                G = g >> 2
                B = b >> 3
                rgb = (R << 11) | (G << 5) | B
                data[pos] = rgb >> 8
                pos += 1
                data[pos] = rgb & 0xFF
                pos += 1
                if transparent:
                    data[pos] = a
                    pos += 1

    elif config[CONF_TYPE] in ["BINARY", "TRANSPARENT_BINARY"]:
        width8 = ((width + 7) // 8) * 8
        data = [0 for _ in range((height * width8 // 8) * frames)]
        for frameIndex in range(frames):
            image.seek(frameIndex)
            if transparent:
                alpha = image.split()[-1]
                has_alpha = alpha.getextrema()[0] < 0xFF
            else:
                has_alpha = False
            frame = image.convert("1", dither=Image.Dither.NONE)
            if CONF_RESIZE in config:
                frame = frame.resize([width, height])
                if transparent:
                    alpha = alpha.resize([width, height])
            for x, y in [(i, j) for i in range(width) for j in range(height)]:
                if transparent and has_alpha:
                    if not alpha.getpixel((x, y)):
                        continue
                elif frame.getpixel((x, y)):
                    continue

                pos = x + y * width8 + (height * width8 * frameIndex)
                data[pos // 8] |= 0x80 >> (pos % 8)
    else:
        raise core.EsphomeError(
            f"Animation f{config[CONF_ID]} has not supported type {config[CONF_TYPE]}."
        )

    rhs = [HexInt(x) for x in data]
    prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
    var = cg.new_Pvariable(
        config[CONF_ID],
        prog_arr,
        width,
        height,
        frames,
        espImage.IMAGE_TYPE[config[CONF_TYPE]],
    )
    cg.add(var.set_transparency(transparent))
    if loop_config := config.get(CONF_LOOP):
        start = loop_config[CONF_START_FRAME]
        end = loop_config.get(CONF_END_FRAME, frames)
        count = loop_config.get(CONF_REPEAT, -1)
        cg.add(var.set_loop(start, end, count))
