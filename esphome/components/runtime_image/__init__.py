from dataclasses import dataclass
import subprocess

import esphome.codegen as cg
from esphome.components.const import CONF_BYTE_ORDER
from esphome.components.image import (
    IMAGE_TYPE,
    Image_,
    validate_byte_order,
    validate_settings,
    validate_transparency,
    validate_type,
)
from esphome.config_helpers import filter_source_files_from_defines
import esphome.config_validation as cv
from esphome.const import CONF_FORMAT, CONF_ID, CONF_RESIZE, CONF_TYPE
from esphome.core import CORE
from esphome.types import ConfigType

AUTO_LOAD = ["image"]
CODEOWNERS = ["@guillempages", "@clydebarrow", "@kahrendt"]
DOMAIN = "runtime_image"

CONF_JPEG_DECODER = "jpeg_decoder"
CONF_PLACEHOLDER = "placeholder"
CONF_TRANSPARENCY = "transparency"

DECODER_JPEGDEC = "JPEGDEC"
DECODER_LIBJPEG_TURBO = "LIBJPEG_TURBO"


@dataclass
class RuntimeImageData:
    """Build-wide runtime_image settings, shared by every image in the config."""

    jpeg_decoder: str = DECODER_JPEGDEC


def _get_data() -> RuntimeImageData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = RuntimeImageData()
    return CORE.data[DOMAIN]


def _host_jpeg_flags() -> list[str]:
    """Compiler and linker flags for the system libjpeg on the host platform."""
    try:
        return (
            subprocess.check_output(
                ["pkg-config", "--cflags", "--libs", "libjpeg"], close_fds=False
            )
            .decode()
            .split()
        )
    except (OSError, subprocess.CalledProcessError):
        # pkg-config is often not installed even where libjpeg is. Fall back to the
        # default search paths; the linker reports a clear error if it is missing.
        return ["-ljpeg"]


runtime_image_ns = cg.esphome_ns.namespace("runtime_image")

# Base decoder classes
ImageDecoder = runtime_image_ns.class_("ImageDecoder")
BmpDecoder = runtime_image_ns.class_("BmpDecoder", ImageDecoder)
JpegDecoder = runtime_image_ns.class_("JpegDecoder", ImageDecoder)
JpegTurboDecoder = runtime_image_ns.class_("JpegTurboDecoder", ImageDecoder)
PngDecoder = runtime_image_ns.class_("PngDecoder", ImageDecoder)
QoiDecoder = runtime_image_ns.class_("QoiDecoder", ImageDecoder)

# Runtime image class
RuntimeImage = runtime_image_ns.class_(
    "RuntimeImage", cg.esphome_ns.namespace("image").class_("Image")
)

# Image format enum
ImageFormat = runtime_image_ns.enum("ImageFormat")
IMAGE_FORMAT_AUTO = ImageFormat.AUTO
IMAGE_FORMAT_BMP = ImageFormat.BMP
IMAGE_FORMAT_JPEG = ImageFormat.JPEG
IMAGE_FORMAT_PNG = ImageFormat.PNG
IMAGE_FORMAT_QOI = ImageFormat.QOI

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


class AUTOFormat(Format):
    """AUTO format - detect from MIME type."""

    def __init__(self) -> None:
        super().__init__("AUTO", None)

    def actions(self) -> None:
        # dict.fromkeys dedupes the JPG/JPEG alias so each format runs once
        for image_format in dict.fromkeys(IMAGE_FORMATS.values()):
            image_format.actions()


class BMPFormat(Format):
    """BMP format decoder configuration."""

    def __init__(self) -> None:
        super().__init__("BMP", BmpDecoder)

    def actions(self) -> None:
        cg.add_define("USE_RUNTIME_IMAGE_BMP")


class JPEGFormat(Format):
    """JPEG format decoder configuration."""

    def __init__(self) -> None:
        super().__init__("JPEG", JpegDecoder)

    def actions(self) -> None:
        cg.add_define("USE_RUNTIME_IMAGE_JPEG")
        if _get_data().jpeg_decoder == DECODER_LIBJPEG_TURBO:
            # libjpeg-turbo supports progressive JPEG images, which JPEGDEC
            # does not.
            cg.add_define("USE_RUNTIME_IMAGE_JPEG_TURBO")
            if CORE.is_host:
                # Host links the system libjpeg rather than building a copy.
                for flag in _host_jpeg_flags():
                    cg.add_build_flag(flag)
                return
            from esphome.components.esp32 import add_idf_component

            # Fetched via git instead of the component registry: the registry
            # checkout is named espressif__libjpeg-turbo, which breaks the
            # component's own reference to the idf::libjpeg-turbo CMake
            # target. A git dependency keeps the plain component name.
            add_idf_component(
                name="libjpeg-turbo",
                repo="https://github.com/espressif/idf-extra-components.git",
                ref="19cc4c48622a6025ef105bd27debd55c80c9a83d",
                path="libjpeg-turbo",
            )
            return
        cg.add_define("USE_RUNTIME_IMAGE_JPEG_DEC")
        cg.add_library("JPEGDEC", "1.8.4", "https://github.com/bitbank2/JPEGDEC#1.8.4")
        if CORE.is_host:
            # JPEGDEC's host detection checks __MACH__/__LINUX__, but gcc only
            # predefines the lowercase __linux__; without this a Linux host
            # build tries to include Arduino.h.
            cg.add_build_flag("-D__LINUX__")
        if CORE.is_esp32:
            from esphome.components.esp32 import add_idf_component

            # JPEGDEC uses ESP32-S3 SIMD optimizations (guarded by board-level
            # ARDUINO_ESP32S3_DEV define) that require esp-dsp headers.
            # On Arduino this overwrites the stub; on IDF it adds the component.
            add_idf_component(name="espressif/esp-dsp", ref="1.8.2")


class PNGFormat(Format):
    """PNG format decoder configuration."""

    def __init__(self) -> None:
        super().__init__("PNG", PngDecoder)

    def actions(self) -> None:
        cg.add_define("USE_RUNTIME_IMAGE_PNG")
        cg.add_library("pngle", "1.1.0")


class QOIFormat(Format):
    """QOI format decoder configuration."""

    def __init__(self):
        super().__init__("QOI", QoiDecoder)

    def actions(self) -> None:
        cg.add_define("USE_RUNTIME_IMAGE_QOI")


# Decodable formats only; platforms that support runtime detection accept
# "AUTO" in their own schema and get_format() resolves it
_JPEG_FORMAT = JPEGFormat()

# Registry of available formats
IMAGE_FORMATS = {
    "BMP": BMPFormat(),
    "JPEG": _JPEG_FORMAT,
    "JPG": _JPEG_FORMAT,  # Alias for JPEG
    "PNG": PNGFormat(),
    "QOI": QOIFormat(),
}

FILTER_SOURCE_FILES = filter_source_files_from_defines(
    {
        "bmp_decoder.cpp": "USE_RUNTIME_IMAGE_BMP",
        "jpeg_decoder.cpp": "USE_RUNTIME_IMAGE_JPEG_DEC",
        "jpeg_turbo_decoder.cpp": "USE_RUNTIME_IMAGE_JPEG_TURBO",
        "png_decoder.cpp": "USE_RUNTIME_IMAGE_PNG",
        "qoi_decoder.cpp": "USE_RUNTIME_IMAGE_QOI",
    }
)

AUTO_FORMAT = AUTOFormat()


def _validate_jpeg_decoder(config: ConfigType) -> ConfigType:
    """Record the build-wide JPEG decoder so every image uses the same one."""
    decoder = config[CONF_JPEG_DECODER]
    # The libjpeg-turbo component is only fetched by the esp-idf build generator, so on
    # the PlatformIO toolchain the dependency is silently dropped and the build fails
    # later on a missing jpeglib.h. Reject it here instead.
    if decoder == DECODER_LIBJPEG_TURBO and not (
        (CORE.is_esp32 and CORE.using_toolchain_esp_idf) or CORE.is_host
    ):
        raise cv.Invalid(
            f"'{CONF_JPEG_DECODER}: {DECODER_LIBJPEG_TURBO}' is only supported on ESP32 "
            "with the esp-idf toolchain, and on host",
            [CONF_JPEG_DECODER],
        )
    _get_data().jpeg_decoder = decoder
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_JPEG_DECODER, default=DECODER_JPEGDEC): cv.one_of(
                DECODER_JPEGDEC, DECODER_LIBJPEG_TURBO, upper=True
            ),
        }
    ),
    _validate_jpeg_decoder,
)


def get_format(format_name: str) -> Format | None:
    """Get a format instance by name."""
    name = format_name.upper()
    if name == "AUTO":
        return AUTO_FORMAT
    return IMAGE_FORMATS.get(name)


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
            cv.Optional(CONF_BYTE_ORDER): validate_byte_order,
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
    # If unspecified, use little endian
    byte_order_big_endian = config.get(CONF_BYTE_ORDER) == "BIG_ENDIAN"

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
