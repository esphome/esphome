"""Sendspin image platform."""

from esphome import automation
import esphome.codegen as cg
from esphome.components import runtime_image
from esphome.components.const import CONF_SLOT
from esphome.components.image import CONF_TRANSPARENCY, Image_, add_metadata
import esphome.config_validation as cv
from esphome.const import (
    CONF_FORMAT,
    CONF_HEIGHT,
    CONF_ID,
    CONF_RESIZE,
    CONF_SOURCE,
    CONF_TYPE,
    CONF_WIDTH,
)
from esphome.core import ID
from esphome.cpp_generator import TemplateArgsType
from esphome.types import ConfigType

from .. import (
    CONF_DISPLAY_OFFSET,
    CONF_SENDSPIN_ID,
    IMAGE_FORMAT_BMP,
    IMAGE_FORMAT_JPEG,
    IMAGE_FORMAT_PNG,
    IMAGE_SOURCE_ALBUM,
    IMAGE_SOURCE_ARTIST,
    SendspinHub,
    register_artwork_preference,
    sendspin_ns,
)

AUTO_LOAD = ["runtime_image"]
CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["sendspin"]

# runtime_image refuses to size a buffer beyond this, so anything larger fails at setup rather
# than at validation. The library's ImageSlotPreference width/height fields are uint16_t, which
# is the looser of the two bounds.
MAX_IMAGE_DIMENSION = 32767

# Sanity bound for display_offset; the library field is int32_t milliseconds and offsets beyond
# a few seconds around the track boundary are meaningless.
MAX_DISPLAY_OFFSET = cv.TimePeriod(seconds=60)
MIN_DISPLAY_OFFSET = cv.TimePeriod(seconds=-60)

CONF_CURRENT_IMAGE = "current_image"
CONF_TRANSITION_IMAGE = "transition_image"
CONF_ON_IMAGE_DISPLAY = "on_image_display"
CONF_ON_IMAGE_CLEAR = "on_image_clear"
CONF_ON_IMAGE_ERROR = "on_image_error"

# Map runtime_image's validated format string to the sendspin library's SendspinImageFormat enum.
# runtime_image accepts "JPG" as an alias for JPEG, so both keys map to the JPEG enum.
_FORMAT_TO_SENDSPIN_ENUM = {
    "JPEG": IMAGE_FORMAT_JPEG,
    "JPG": IMAGE_FORMAT_JPEG,
    "PNG": IMAGE_FORMAT_PNG,
    "BMP": IMAGE_FORMAT_BMP,
}

# The library's SendspinImageSource::NONE is its internal "unset" sentinel; a slot advertising it
# would never receive artwork while still paying for two frame buffers, so it is not offered here.
IMAGE_SOURCES = {
    "ALBUM": IMAGE_SOURCE_ALBUM,
    "ARTIST": IMAGE_SOURCE_ARTIST,
}

# The platform entry configures an artwork slot; the images it shows are declared inside it. The
# slot itself is the automation target (triggers and the transition_finished action).
SendspinImageSlot = sendspin_ns.class_(
    "SendspinImageSlot",
    cg.Component,
    cg.Parented.template(SendspinHub),
)
ArtworkImageView = sendspin_ns.class_("ArtworkImageView", Image_)

# A dict rather than a bare ID so per-image options can be added later without a new top-level key.
_IMAGE_SCHEMA = cv.Schema({cv.Required(CONF_ID): cv.declare_id(ArtworkImageView)})

_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_IMAGE_DISPLAY,
        "add_on_image_display_callback",
        [(cg.uint32, "lateness_ms")],
    ),
    automation.CallbackAutomation(CONF_ON_IMAGE_CLEAR, "add_on_image_clear_callback"),
    automation.CallbackAutomation(CONF_ON_IMAGE_ERROR, "add_on_image_error_callback"),
)


def _assign_slot_and_register(config: ConfigType) -> ConfigType:
    """Register the artwork preference with the hub and record the slot it was given."""
    width, height = config[CONF_RESIZE]
    if width > MAX_IMAGE_DIMENSION or height > MAX_IMAGE_DIMENSION:
        raise cv.Invalid(
            f"'{CONF_RESIZE}' width and height must be {MAX_IMAGE_DIMENSION} or less",
            path=[CONF_RESIZE],
        )

    config[CONF_SLOT] = register_artwork_preference(
        {
            CONF_SOURCE: config[CONF_SOURCE],
            CONF_FORMAT: _FORMAT_TO_SENDSPIN_ENUM[config[CONF_FORMAT]],
            CONF_WIDTH: width,
            CONF_HEIGHT: height,
            CONF_DISPLAY_OFFSET: config[CONF_DISPLAY_OFFSET].total_milliseconds,
        }
    )
    return config


# The format, type, resize, transparency, byte order and placeholder keys all describe the slot:
# they set what is requested from the server and how it is decoded, not either individual image.
# Only the IDs are per-image, so runtime_image_schema declares the slot itself.
CONFIG_SCHEMA = cv.All(
    runtime_image.runtime_image_schema(SendspinImageSlot).extend(
        {
            cv.GenerateID(): cv.declare_id(SendspinImageSlot),
            cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub),
            # Narrow runtime_image's format list to what the library can request, so the
            # accepted set and the enum map below cannot drift apart.
            cv.Required(CONF_FORMAT): cv.one_of(*_FORMAT_TO_SENDSPIN_ENUM, upper=True),
            cv.Required(CONF_RESIZE): cv.dimensions,
            cv.Required(CONF_CURRENT_IMAGE): _IMAGE_SCHEMA,
            cv.Optional(CONF_TRANSITION_IMAGE): _IMAGE_SCHEMA,
            cv.Optional(CONF_SOURCE, default="ALBUM"): cv.enum(
                IMAGE_SOURCES, upper=True
            ),
            # Positive fires on_image_display before the server's display timestamp (negative
            # delays it), so a cross-fade can straddle the track boundary.
            cv.Optional(CONF_DISPLAY_OFFSET, default="0ms"): cv.All(
                cv.time_period,
                # The library field is whole milliseconds; reject finer values rather than
                # silently rounding them down to zero.
                cv.time_period_in_milliseconds_,
                cv.Range(min=MIN_DISPLAY_OFFSET, max=MAX_DISPLAY_OFFSET),
            ),
            cv.Optional(CONF_ON_IMAGE_DISPLAY): automation.validate_automation({}),
            cv.Optional(CONF_ON_IMAGE_CLEAR): automation.validate_automation({}),
            cv.Optional(CONF_ON_IMAGE_ERROR): automation.validate_automation({}),
        }
    ),
    runtime_image.validate_runtime_image_settings,
    cv.only_on_esp32,
    _assign_slot_and_register,
)


async def to_code(config: ConfigType) -> None:
    settings = await runtime_image.process_runtime_image_config(config)

    def make_view(view_id: ID) -> cg.MockObj:
        # Views start with no frame; the slot points them at its buffers in setup(). The size is
        # given up front so the view is well formed before then. LVGL picks it up from the first
        # lvgl.image.update in on_image_display, not from the widget's initial src: at that point
        # the view still has no frame, so its descriptor is empty.
        view = cg.new_Pvariable(
            view_id,
            cg.nullptr,
            settings.width,
            settings.height,
            settings.image_type_enum,
            settings.transparent,
        )
        add_metadata(
            view_id,
            settings.width,
            settings.height,
            config[CONF_TYPE],
            config[CONF_TRANSPARENCY],
        )
        return view

    current_image = make_view(config[CONF_CURRENT_IMAGE][CONF_ID])
    if settings.placeholder is not None:
        cg.add(current_image.set_placeholder(settings.placeholder))

    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_SLOT],
        current_image,
        settings.width,
        settings.height,
        settings.format_enum,
        settings.image_type_enum,
        settings.transparent,
        settings.byte_order_big_endian,
    )
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SENDSPIN_ID])

    if (transition_image := config.get(CONF_TRANSITION_IMAGE)) is not None:
        cg.add(var.set_transition_image(make_view(transition_image[CONF_ID])))

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


SendspinImageTransitionFinishedAction = sendspin_ns.class_(
    "SendspinImageTransitionFinishedAction",
    automation.Action,
    cg.Parented.template(SendspinImageSlot),
)


@automation.register_action(
    "sendspin.image.transition_finished",
    SendspinImageTransitionFinishedAction,
    automation.maybe_simple_id(
        cv.Schema(
            {
                cv.GenerateID(): cv.use_id(SendspinImageSlot),
            }
        )
    ),
    synchronous=True,
)
async def sendspin_image_transition_finished_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: cg.TemplateArguments,
    args: TemplateArgsType,
) -> cg.MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var
