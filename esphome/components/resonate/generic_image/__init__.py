"""Resonate Image Setup."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import runtime_image
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TRIGGER_ID

from .. import CONF_RESONATE_ID, ResonateHub, resonate_ns

AUTO_LOAD = ["runtime_image"]
CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["resonate"]

CONF_ON_IMAGE_RECEIVED = "on_image_received"
CONF_ON_IMAGE_DECODED = "on_image_decoded"
CONF_ON_IMAGE_ERROR = "on_image_error"

ResonateImage = resonate_ns.class_(
    "ResonateImage",
    runtime_image.RuntimeImage,
    cg.Component,
)

# Trigger classes for automation
ResonateImageReceivedTrigger = resonate_ns.class_(
    "ResonateImageReceivedTrigger", automation.Trigger.template()
)
ResonateImageDecodedTrigger = resonate_ns.class_(
    "ResonateImageDecodedTrigger", automation.Trigger.template()
)
ResonateImageErrorTrigger = resonate_ns.class_(
    "ResonateImageErrorTrigger", automation.Trigger.template()
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

# CONFIG_SCHEMA = cv.All(
#     runtime_image.runtime_image_schema(ResonateImage),
#     runtime_image.validate_runtime_image_settings,
# )

CONFIG_SCHEMA = cv.All(
    runtime_image.runtime_image_schema(ResonateImage).extend(
        {
            cv.GenerateID(): cv.declare_id(ResonateImage),
            cv.GenerateID(CONF_RESONATE_ID): cv.use_id(ResonateHub),
            cv.Optional(CONF_ON_IMAGE_RECEIVED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        ResonateImageReceivedTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_IMAGE_DECODED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        ResonateImageDecodedTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_IMAGE_ERROR): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        ResonateImageErrorTrigger
                    ),
                }
            ),
        }
    ),
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
    await cg.register_parented(var, config[CONF_RESONATE_ID])

    # Register automation triggers
    for conf in config.get(CONF_ON_IMAGE_RECEIVED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_IMAGE_DECODED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_IMAGE_ERROR, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
