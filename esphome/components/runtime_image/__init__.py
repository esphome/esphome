import esphome.codegen as cg

AUTO_LOAD = ["image"]
CODEOWNERS = ["@guillempages", "@clydebarrow", "@kahrendt"]

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

    def __init__(self, name, decoder_class):
        self.name = name
        self.decoder_class = decoder_class

    def actions(self):
        """Add defines and libraries needed for this format."""
        pass


class BMPFormat(Format):
    """BMP format decoder configuration."""

    def __init__(self):
        super().__init__("BMP", BmpDecoder)

    def actions(self):
        cg.add_define("USE_RUNTIME_IMAGE_BMP")


class JPEGFormat(Format):
    """JPEG format decoder configuration."""

    def __init__(self):
        super().__init__("JPEG", JpegDecoder)

    def actions(self):
        cg.add_define("USE_RUNTIME_IMAGE_JPEG")
        cg.add_library("JPEGDEC", None, "https://github.com/bitbank2/JPEGDEC#ca1e0f2")


class PNGFormat(Format):
    """PNG format decoder configuration."""

    def __init__(self):
        super().__init__("PNG", PngDecoder)

    def actions(self):
        cg.add_define("USE_RUNTIME_IMAGE_PNG")
        cg.add_library("pngle", "1.1.0")


# Registry of available formats
IMAGE_FORMATS = {
    "BMP": BMPFormat(),
    "JPEG": JPEGFormat(),
    "PNG": PNGFormat(),
    "JPG": JPEGFormat(),  # Alias for JPEG
}


def get_format(format_name):
    """Get a format instance by name."""
    return IMAGE_FORMATS.get(format_name.upper())


def enable_format(format_name):
    """Enable a specific image format by adding its defines and libraries."""
    format_obj = get_format(format_name)
    if format_obj:
        format_obj.actions()
        return format_obj
    return None
