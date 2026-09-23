from __future__ import annotations

import contextlib
import hashlib
import io
import logging
from pathlib import Path
import re

from PIL import Image, UnidentifiedImageError

from esphome import core, external_files
import esphome.codegen as cg
from esphome.components.const import CONF_BYTE_ORDER
from esphome.components.image import (
    CONF_INVERT_ALPHA,
    CONF_OPAQUE,
    CONF_TRANSPARENCY,
    DOMAIN,
    IMAGE_TYPE,
    Image_,
    ImageEncoder,
    add_metadata,
    get_image_type_enum,
    get_transparency_enum,
    is_svg_file,
    validate_settings,
    validate_transparency,
    validate_type,
)
import esphome.config_validation as cv
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
    CONF_URL,
)
from esphome.core import CORE, HexInt
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]

_LOGGER = logging.getLogger(__name__)

# If the MDI file cannot be downloaded within this time, abort.
IMAGE_DOWNLOAD_TIMEOUT = 30  # seconds

SOURCE_LOCAL = "local"
SOURCE_WEB = "web"

SOURCE_MDI = "mdi"
SOURCE_MDIL = "mdil"
SOURCE_MEMORY = "memory"

MDI_SOURCES = {
    SOURCE_MDI: "https://raw.githubusercontent.com/Templarian/MaterialDesign/master/svg/",
    SOURCE_MDIL: "https://raw.githubusercontent.com/Pictogrammers/MaterialDesignLight/refs/heads/master/svg/",
    SOURCE_MEMORY: "https://raw.githubusercontent.com/Pictogrammers/Memory/refs/heads/main/src/svg/",
}


def compute_local_image_path(value) -> Path:
    url = value[CONF_URL] if isinstance(value, dict) else value
    h = hashlib.new("sha256")
    h.update(url.encode())
    key = h.hexdigest()[:8]
    # Downloaded files are cached under the shared `image` domain directory so
    # the cache location is unaffected by which platform requested the file.
    base_dir = external_files.compute_local_file_dir(DOMAIN)
    return base_dir / key


def local_path(value):
    value = value[CONF_PATH] if isinstance(value, dict) else value
    return str(CORE.relative_config_path(value))


def download_file(url, path):
    external_files.download_content(url, path, IMAGE_DOWNLOAD_TIMEOUT)
    return str(path)


def download_gh_svg(value, source):
    mdi_id = value[CONF_ICON] if isinstance(value, dict) else value
    base_dir = external_files.compute_local_file_dir(DOMAIN) / source
    path = base_dir / f"{mdi_id}.svg"

    url = MDI_SOURCES[source] + mdi_id + ".svg"
    return download_file(url, path)


def download_image(value):
    value = value[CONF_URL] if isinstance(value, dict) else value
    return download_file(value, compute_local_image_path(value))


def validate_file_shorthand(value):
    value = cv.string_strict(value)
    parts = value.strip().split(":")
    if len(parts) == 2 and parts[0] in MDI_SOURCES:
        match = re.match(r"^[a-zA-Z0-9\-]+$", parts[1])
        if match is None:
            raise cv.Invalid(f"Could not parse mdi icon name from '{value}'.")
        return download_gh_svg(parts[1], parts[0])

    if value.startswith(("http://", "https://")):
        return download_image(value)

    value = cv.file_(value)
    return local_path(value)


LOCAL_SCHEMA = cv.All(
    {
        cv.Required(CONF_PATH): cv.file_,
    },
    local_path,
)


def mdi_schema(source):
    def validate_mdi(value):
        return download_gh_svg(value, source)

    return cv.All(
        cv.Schema(
            {
                cv.Required(CONF_ICON): cv.string,
            }
        ),
        validate_mdi,
    )


WEB_SCHEMA = cv.All(
    {
        cv.Required(CONF_URL): cv.string,
    },
    download_image,
)


TYPED_FILE_SCHEMA = cv.typed_schema(
    {
        SOURCE_LOCAL: LOCAL_SCHEMA,
        SOURCE_WEB: WEB_SCHEMA,
    }
    | {source: mdi_schema(source) for source in MDI_SOURCES},
    key=CONF_SOURCE,
)


OPTIONS_SCHEMA = {
    cv.Optional(CONF_RESIZE): cv.dimensions,
    cv.Optional(CONF_DITHER, default="NONE"): cv.one_of(
        "NONE", "FLOYDSTEINBERG", upper=True
    ),
    cv.Optional(CONF_INVERT_ALPHA, default=False): cv.boolean,
    cv.Optional(CONF_BYTE_ORDER): cv.one_of("BIG_ENDIAN", "LITTLE_ENDIAN", upper=True),
    cv.Optional(CONF_TRANSPARENCY, default=CONF_OPAQUE): validate_transparency(),
}


def image_schema(class_: MockObjClass = Image_) -> cv.Schema:
    """Build the validation schema for a single file-backed image entry.

    Shared by the built-in ``file`` image platform and the ``animation``
    platform (which extends it). Platforms that source their pixels elsewhere
    (e.g. ``online_image``) provide their own schema instead.

    :param class_: The declared C++ class for the generated image instance.
    """
    return cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(class_),
            cv.Required(CONF_FILE): cv.Any(validate_file_shorthand, TYPED_FILE_SCHEMA),
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
            **OPTIONS_SCHEMA,
            cv.Required(CONF_TYPE): validate_type(IMAGE_TYPE),
        }
    )


def validate_image_final(config: ConfigType) -> ConfigType:
    """Per-entry final validation, shared by file-backed image platforms.

    For LVGL 9 the default byte order for RGB565 images is little-endian, so
    fill in that default when the user did not specify a byte order and warn
    when big-endian was explicitly requested.
    """
    if byte_order := config.get(CONF_BYTE_ORDER):
        if byte_order == "BIG_ENDIAN":
            _LOGGER.warning(
                "The image '%s' is configured with big-endian byte order, little-endian is expected",
                config.get(CONF_FILE),
            )
    else:
        config[CONF_BYTE_ORDER] = "LITTLE_ENDIAN"
    return config


async def new_image(config: ConfigType) -> MockObj:
    """Generate a single file-backed ``image::Image`` instance.

    Used by the built-in ``file`` platform; encodes the image data, registers
    the C++ variable and records its metadata for other components to consume.
    """
    prog_arr, width, height, image_type, trans_value, _ = await write_image(config)
    var = cg.new_Pvariable(
        config[CONF_ID], prog_arr, width, height, image_type, trans_value
    )
    add_metadata(
        config[CONF_ID], width, height, config[CONF_TYPE], config[CONF_TRANSPARENCY]
    )
    return var


async def write_image(config, all_frames=False):
    path = Path(config[CONF_FILE])
    if not path.is_file():
        raise core.EsphomeError(f"Could not load image file {path}")

    resize = config.get(CONF_RESIZE)
    try:
        if is_svg_file(path):
            import resvg_py

            resize = resize or (None, None)
            image_data = resvg_py.svg_to_bytes(
                svg_path=str(path), width=resize[0], height=resize[1], dpi=100
            )

            # Convert bytes to Pillow Image
            image = Image.open(io.BytesIO(image_data))
            width, height = image.size

        else:
            image = Image.open(path)
            width, height = image.size
            if resize:
                # Preserve aspect ratio
                new_width_max = min(width, resize[0])
                new_height_max = min(height, resize[1])
                ratio = min(new_width_max / width, new_height_max / height)
                width, height = int(width * ratio), int(height * ratio)
    except (OSError, UnidentifiedImageError, ValueError) as exc:
        raise core.EsphomeError(f"Could not read image file {path}: {exc}") from exc

    if not resize and (width > 500 or height > 500):
        _LOGGER.warning(
            'The image "%s" you requested is very big. Please consider'
            " using the resize parameter.",
            path,
        )

    dither = (
        Image.Dither.NONE
        if config[CONF_DITHER] == "NONE"
        else Image.Dither.FLOYDSTEINBERG
    )
    type = config[CONF_TYPE]
    transparency = config.get(CONF_TRANSPARENCY, CONF_OPAQUE)
    invert_alpha = config[CONF_INVERT_ALPHA]
    frame_count = 1
    if all_frames:
        with contextlib.suppress(AttributeError):
            frame_count = image.n_frames
        if frame_count <= 1:
            _LOGGER.warning("Image file %s has no animation frames", path)

    # Encode each frame with its own encoder and concatenate. This keeps every
    # frame self-contained on disk (e.g. RGB565+alpha emits [RGB plane | alpha plane]
    # per frame) so animation frame stepping in image.cpp / animation.cpp stays
    # correct without needing to know the total frame count.
    byte_order = config.get(CONF_BYTE_ORDER)
    combined_data: list[int] = []
    encoder: ImageEncoder | None = None
    for frame_index in range(frame_count):
        image.seek(frame_index)
        encoder = IMAGE_TYPE[type](width, height, transparency, dither, invert_alpha)
        if byte_order is not None:
            # Check for valid type has already been done in validate_settings
            encoder.set_big_endian(byte_order == "BIG_ENDIAN")
        pixels = encoder.convert(image.resize((width, height)), path).getdata()
        for row in range(height):
            for col in range(width):
                encoder.encode(pixels[row * width + col])
            encoder.end_row()
        encoder.end_image()
        combined_data.extend(encoder.data)

    rhs = [HexInt(x) for x in combined_data]
    prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
    image_type = get_image_type_enum(type)
    trans_value = get_transparency_enum(encoder.transparency)

    return prog_arr, width, height, image_type, trans_value, frame_count


# The built-in static-image platform: pixels embedded at compile time from a
# local file, a downloaded web image, or a Material Design Icon.
CONFIG_SCHEMA = cv.All(image_schema(Image_), validate_settings)

FINAL_VALIDATE_SCHEMA = validate_image_final


async def to_code(config: ConfigType) -> None:
    await new_image(config)
