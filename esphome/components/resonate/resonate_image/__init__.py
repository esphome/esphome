"""Resonate Image Setup."""

import esphome.codegen as cg
from esphome.components import runtime_image
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import resonate_ns

AUTO_LOAD = ["runtime_image"]
CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["resonate"]
MULTI_CONF = True

ResonateImage = resonate_ns.class_(
    "ResonateImage",
    runtime_image.RuntimeImage,
    cg.Component,
)

# Use ImageFormat from runtime_image
ImageFormat = runtime_image.ImageFormat

# Map format names to ImageFormat enum values for backward compatibility
IMAGE_FORMATS = {
    "BMP": "BMP",
    "JPEG": "JPEG",
    "PNG": "PNG",
    "JPG": "JPEG",  # Alias for JPEG
}

CONFIG_SCHEMA = cv.All(
    runtime_image.create_runtime_image_schema(ResonateImage),
    runtime_image.validate_runtime_image_settings,
)


async def to_code(config):
    cg.add_define("USE_RESONATE_IMAGE", True)

    # Use the enhanced helper function to get all runtime image parameters
    (
        width,
        height,
        format_enum,
        image_type_enum,
        transparent,
        byte_order_big_endian,
        placeholder,
    ) = await runtime_image.process_runtime_image_config(config)

    var = cg.new_Pvariable(
        config[CONF_ID],
        width,
        height,
        format_enum,
        image_type_enum,
        transparent,
        byte_order_big_endian,
        placeholder,
    )
    await cg.register_component(var, config)
