"""Sendspin generic_image platform."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import runtime_image
from esphome.components.image import CONF_TRANSPARENCY, add_metadata
import esphome.config_validation as cv
from esphome.const import (
    CONF_FORMAT,
    CONF_HEIGHT,
    CONF_ID,
    CONF_RESIZE,
    CONF_SOURCE,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    CONF_WIDTH,
)
from esphome.core import CORE
from esphome.types import ConfigType

from .. import (
    CONF_SENDSPIN_ID,
    CONF_SLOT,
    IMAGE_FORMAT_BMP,
    IMAGE_FORMAT_JPEG,
    IMAGE_FORMAT_PNG,
    IMAGE_SOURCE_ALBUM,
    IMAGE_SOURCE_ARTIST,
    IMAGE_SOURCE_NONE,
    SendspinHub,
    register_artwork_preference,
    sendspin_ns,
)

AUTO_LOAD = ["runtime_image"]
CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["sendspin"]

MAX_IMAGE_SLOTS = 4
_SLOT_COUNTER_KEY = "sendspin_image_slot_counter"

CONF_ON_IMAGE_DISPLAY = "on_image_display"
CONF_ON_IMAGE_ERROR = "on_image_error"

# Map runtime_image's format string to the sendspin library's SendspinImageFormat enum.
_FORMAT_TO_SENDSPIN_ENUM = {
    "JPEG": IMAGE_FORMAT_JPEG,
    "PNG": IMAGE_FORMAT_PNG,
    "BMP": IMAGE_FORMAT_BMP,
}

IMAGE_SOURCES = {
    "ALBUM": IMAGE_SOURCE_ALBUM,
    "ARTIST": IMAGE_SOURCE_ARTIST,
    "NONE": IMAGE_SOURCE_NONE,
}

SendspinImage = sendspin_ns.class_(
    "SendspinImage",
    runtime_image.RuntimeImage,
    cg.Component,
)

SendspinImageDisplayTrigger = sendspin_ns.class_(
    "SendspinImageDisplayTrigger", automation.Trigger.template()
)
SendspinImageErrorTrigger = sendspin_ns.class_(
    "SendspinImageErrorTrigger", automation.Trigger.template()
)


def _assign_slot_and_register(config: ConfigType) -> ConfigType:
    """Auto-assign a slot, validate the max count, and register the artwork preference with the hub."""
    current = CORE.data.get(_SLOT_COUNTER_KEY, 0)
    if current >= MAX_IMAGE_SLOTS:
        raise cv.Invalid(
            f"Too many Sendspin generic_image components. Maximum is {MAX_IMAGE_SLOTS}."
        )
    CORE.data[_SLOT_COUNTER_KEY] = current + 1
    config[CONF_SLOT] = current

    width, height = config[CONF_RESIZE]
    register_artwork_preference(
        {
            CONF_SLOT: current,
            CONF_SOURCE: config[CONF_SOURCE],
            CONF_FORMAT: _FORMAT_TO_SENDSPIN_ENUM[config[CONF_FORMAT]],
            CONF_WIDTH: width,
            CONF_HEIGHT: height,
        }
    )
    return config


CONFIG_SCHEMA = cv.All(
    runtime_image.runtime_image_schema(SendspinImage).extend(
        {
            cv.GenerateID(): cv.declare_id(SendspinImage),
            cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub),
            cv.Required(CONF_RESIZE): cv.dimensions,
            cv.Optional(CONF_SOURCE, default="ALBUM"): cv.enum(
                IMAGE_SOURCES, upper=True
            ),
            cv.Optional(CONF_ON_IMAGE_DISPLAY): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        SendspinImageDisplayTrigger
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
    cv.only_on_esp32,
    _assign_slot_and_register,
)


async def to_code(config: ConfigType) -> None:
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

    cg.add(var.set_slot(config[CONF_SLOT]))
    cg.add(var.set_image_source(IMAGE_SOURCES[config[CONF_SOURCE]]))

    for conf in config.get(CONF_ON_IMAGE_DISPLAY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_IMAGE_ERROR, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
