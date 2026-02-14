from dataclasses import dataclass

import esphome.codegen as cg
from esphome.components.const import CONF_BYTE_ORDER
from esphome.components.image import (
    IMAGE_TYPE,
    Image_,
    validate_settings,
    validate_transparency,
    validate_type,
)
import esphome.config_validation as cv
from esphome.const import CONF_FORMAT, CONF_ID, CONF_RESIZE, CONF_TYPE

AUTO_LOAD = ["image"]
CODEOWNERS = ["@guillempages", "@clydebarrow", "@kahrendt"]

CONF_PLACEHOLDER = "placeholder"
CONF_TRANSPARENCY = "transparency"

runtime_image_ns = cg.esphome_ns.namespace("runtime_image")

# Base decoder classes
ImageDecoder = runtime_image_ns.class_("ImageDecoder")
BmpDecoder = runtime_image_ns.class_("BmpDecoder", ImageDecoder)
JpegDecoder = runtime_image_ns.class_("JpegDecoder", ImageDecoder)
PngDecoder = runtime_image_ns.class_("PngDecoder", ImageDecoder)

# Runtime image class
RuntimeImage = runtime_image_ns.class_(
    "RuntimeImage", cg.esphome_ns.namespace("image").class_("Image")
)

# Image format enum
ImageFormat = runtime_image_ns.enum("ImageFormat")
IMAGE_FORMAT_AUTO = ImageFormat.AUTO
IMAGE_FORMAT_JPEG = ImageFormat.JPEG
IMAGE_FORMAT_PNG = ImageFormat.PNG
IMAGE_FORMAT_BMP = ImageFormat.BMP

# Export enum for decode errors
DecodeError = runtime_image_ns.enum("DecodeError")
DECODE_ERROR_INVALID_TYPE = DecodeError.DECODE_ERROR_INVALID_TYPE
DECODE_ERROR_UNSUPPORTED_FORMAT = DecodeError.DECODE_ERROR_UNSUPPORTED_FORMAT
DECODE_ERROR_OUT_OF_MEMORY = DecodeError.DECODE_ERROR_OUT_OF_MEMORY


class Format:
    """Base class for image format definitions."""

    def __init__(self, name: str, decoder_class: cg.MockObjClass) -> None:
        self.name = name
        self.decoder_class = decoder_class

    def actions(self) -> None:
        """Add defines and libraries needed for this format."""


class BMPFormat(Format):
    """BMP format decoder configuration."""

    def __init__(self):
        super().__init__("BMP", BmpDecoder)

    def actions(self) -> None:
        cg.add_define("USE_RUNTIME_IMAGE_BMP")


class JPEGFormat(Format):
    """JPEG format decoder configuration."""

    def __init__(self):
        super().__init__("JPEG", JpegDecoder)

    def actions(self) -> None:
        cg.add_define("USE_RUNTIME_IMAGE_JPEG")
        cg.add_library("JPEGDEC", None, "https://github.com/bitbank2/JPEGDEC#ca1e0f2")


class PNGFormat(Format):
    """PNG format decoder configuration."""

    def __init__(self):
        super().__init__("PNG", PngDecoder)

    def actions(self) -> None:
        cg.add_define("USE_RUNTIME_IMAGE_PNG")
        cg.add_library("pngle", "1.1.0")


# Registry of available formats
IMAGE_FORMATS = {
    "BMP": BMPFormat(),
    "JPEG": JPEGFormat(),
    "PNG": PNGFormat(),
    "JPG": JPEGFormat(),  # Alias for JPEG
}


def get_format(format_name: str) -> Format | None:
    """Get a format instance by name."""
    return IMAGE_FORMATS.get(format_name.upper())


def enable_format(format_name: str) -> Format | None:
    """Enable a specific image format by adding its defines and libraries."""
    format_obj = get_format(format_name)
    if format_obj:
        format_obj.actions()
        return format_obj
    return None


# Runtime image configuration schema base - to be extended by components
def runtime_image_schema(image_class: cg.MockObjClass = RuntimeImage) -> cv.Schema:
    """Create a runtime image schema with the specified image class."""
    return cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(image_class),
            cv.Required(CONF_FORMAT): cv.one_of(*IMAGE_FORMATS, upper=True),
            cv.Optional(CONF_RESIZE): cv.dimensions,
            cv.Required(CONF_TYPE): validate_type(IMAGE_TYPE),
            cv.Optional(CONF_BYTE_ORDER): cv.one_of(
                "BIG_ENDIAN", "LITTLE_ENDIAN", upper=True
            ),
            cv.Optional(CONF_TRANSPARENCY, default="OPAQUE"): validate_transparency(),
            cv.Optional(CONF_PLACEHOLDER): cv.use_id(Image_),
        }
    )


def validate_runtime_image_settings(config: dict) -> dict:
    """Apply validate_settings from image component to runtime image config."""
    return validate_settings(config)


@dataclass
class RuntimeImageSettings:
    """Processed runtime image configuration parameters."""

    width: int
    height: int
    format_enum: cg.MockObj
    image_type_enum: cg.MockObj
    transparent: cg.MockObj
    byte_order_big_endian: bool
    placeholder: cg.MockObj | None


async def process_runtime_image_config(config: dict) -> RuntimeImageSettings:
    """
    Helper function to process common runtime image configuration parameters.
    Handles format enabling and returns all necessary enums and parameters.
    """
    from esphome.components.image import get_image_type_enum, get_transparency_enum

    # Get resize dimensions with default (0, 0)
    width, height = config.get(CONF_RESIZE, (0, 0))

    # Handle format (required for runtime images)
    format_name = config[CONF_FORMAT]
    # Enable the format in the runtime_image component
    enable_format(format_name)
    # Map format names to enum values (handle JPG as alias for JPEG)
    if format_name.upper() == "JPG":
        format_name = "JPEG"
    format_enum = getattr(ImageFormat, format_name.upper())

    # Get image type enum
    image_type_enum = get_image_type_enum(config[CONF_TYPE])

    # Get transparency enum
    transparent = get_transparency_enum(config.get(CONF_TRANSPARENCY, "OPAQUE"))

    # Get byte order (True for big endian, False for little endian)
    byte_order_big_endian = config.get(CONF_BYTE_ORDER) != "LITTLE_ENDIAN"

    # Get placeholder if specified
    placeholder = None
    if placeholder_id := config.get(CONF_PLACEHOLDER):
        placeholder = await cg.get_variable(placeholder_id)

    return RuntimeImageSettings(
        width=width,
        height=height,
        format_enum=format_enum,
        image_type_enum=image_type_enum,
        transparent=transparent,
        byte_order_big_endian=byte_order_big_endian,
        placeholder=placeholder,
    )
