"""Sendspin Image Setup."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import runtime_image
from esphome.components.image import CONF_TRANSPARENCY, add_metadata
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_RESIZE, CONF_TRIGGER_ID, CONF_TYPE
from esphome.core import CORE

from .. import CONF_SENDSPIN_ID, SendspinHub, sendspin_ns

CONF_IMAGE_SOURCE = "image_source"

# Keys for CORE.data storage
SENDSPIN_IMAGE_SLOT_KEY = "sendspin_image_slot_counter"
MAX_IMAGE_SLOTS = 4


def _get_next_slot() -> int:
    """Get and increment the next available slot number."""
    current = CORE.data.get(SENDSPIN_IMAGE_SLOT_KEY, 0)
    if current >= MAX_IMAGE_SLOTS:
        raise cv.Invalid(
            f"Too many Sendspin generic_image components. Maximum is {MAX_IMAGE_SLOTS}."
        )
    CORE.data[SENDSPIN_IMAGE_SLOT_KEY] = current + 1
    return current


AUTO_LOAD = ["runtime_image"]
CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["sendspin"]

CONF_ON_IMAGE_RECEIVED = "on_image_received"
CONF_ON_IMAGE_DECODED = "on_image_decoded"
CONF_ON_IMAGE_ERROR = "on_image_error"

SendspinImage = sendspin_ns.class_(
    "SendspinImage",
    runtime_image.RuntimeImage,
    cg.Component,
)

# Trigger classes for automation
SendspinImageReceivedTrigger = sendspin_ns.class_(
    "SendspinImageReceivedTrigger", automation.Trigger.template()
)
SendspinImageDecodedTrigger = sendspin_ns.class_(
    "SendspinImageDecodedTrigger", automation.Trigger.template()
)
SendspinImageErrorTrigger = sendspin_ns.class_(
    "SendspinImageErrorTrigger", automation.Trigger.template()
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

# SendspinImageSource enum from protocol (enum class)
SendspinImageSource = sendspin_ns.enum("SendspinImageSource", is_class=True)

IMAGE_SOURCES = {
    "ALBUM": SendspinImageSource.ALBUM,
    "ARTIST": SendspinImageSource.ARTIST,
    "NONE": SendspinImageSource.NONE,
}

# CONFIG_SCHEMA = cv.All(
#     runtime_image.runtime_image_schema(SendspinImage),
#     runtime_image.validate_runtime_image_settings,
# )

CONFIG_SCHEMA = cv.All(
    runtime_image.runtime_image_schema(SendspinImage).extend(
        {
            cv.GenerateID(): cv.declare_id(SendspinImage),
            cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub),
            cv.Required(CONF_RESIZE): cv.dimensions,
            cv.Optional(CONF_IMAGE_SOURCE, default="ALBUM"): cv.enum(
                IMAGE_SOURCES, upper=True
            ),
            cv.Optional(CONF_ON_IMAGE_RECEIVED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        SendspinImageReceivedTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_IMAGE_DECODED): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        SendspinImageDecodedTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_IMAGE_ERROR): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        SendspinImageErrorTrigger
                    ),
                }
            ),
        }
    ),
    runtime_image.validate_runtime_image_settings,
)


async def to_code(config):
    cg.add_define("USE_SENDSPIN_ARTWORK", True)

    # Use the helper function to get all runtime image parameters
    settings = await runtime_image.process_runtime_image_config(config)

    add_metadata(
        config[CONF_ID],
        settings.width,
        settings.height,
        config[CONF_TYPE],
        config[CONF_TRANSPARENCY],
    )

    var = cg.new_Pvariable(
        config[CONF_ID],
        settings.width,
        settings.height,
        settings.format_enum,
        settings.image_type_enum,
        settings.transparent,
        settings.byte_order_big_endian,
        settings.placeholder,
    )
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SENDSPIN_ID])

    # Set image source and auto-assign slot
    cg.add(var.set_image_source(config[CONF_IMAGE_SOURCE]))
    slot = _get_next_slot()
    cg.add(var.set_slot(slot))

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
