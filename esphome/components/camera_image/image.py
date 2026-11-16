"""Camera image platform: expose the camera's frames as an ``image``."""

from struct import pack, unpack

from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32_camera, runtime_image
from esphome.components.image import CONF_ALPHA_CHANNEL, CONF_TRANSPARENCY, add_metadata
from esphome.components.runtime_image import CONF_DOUBLE_BUFFER
import esphome.config_validation as cv
from esphome.const import (
    CONF_FORMAT,
    CONF_HEIGHT,
    CONF_ID,
    CONF_ON_ERROR,
    CONF_RESIZE,
    CONF_RESOLUTION,
    CONF_ROTATION,
    CONF_TYPE,
    CONF_WIDTH,
    CONF_X,
    CONF_Y,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

AUTO_LOAD = ["runtime_image"]
DEPENDENCIES = ["esp32_camera"]
CODEOWNERS = ["@nliaudat"]

CONF_CAMERA_ID = "camera_id"
CONF_CROP = "crop"
CONF_ON_IMAGE = "on_image"

_CROP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_X): cv.int_range(min=0, max=8192),
        cv.Required(CONF_Y): cv.int_range(min=0, max=8192),
        cv.Required(CONF_WIDTH): cv.int_range(min=1, max=8192),
        cv.Required(CONF_HEIGHT): cv.int_range(min=1, max=8192),
    }
)

camera_image_ns = cg.esphome_ns.namespace("camera_image")
CameraImage = camera_image_ns.class_(
    "CameraImage", cg.PollingComponent, runtime_image.RuntimeImage
)


def _validate_options(config: ConfigType) -> ConfigType:
    transforms = CONF_CROP in config or config[CONF_ROTATION] != 0
    if transforms and config[CONF_DOUBLE_BUFFER]:
        raise cv.Invalid(
            "'crop' and 'rotation' cannot be combined with 'double_buffer'"
        )
    if transforms and config[CONF_TYPE] not in ("GRAYSCALE", "RGB565", "RGB"):
        raise cv.Invalid("'crop' and 'rotation' require type GRAYSCALE, RGB565 or RGB")
    # With an alpha channel the image keeps a separate plane next to the colour
    # data, and crop/rotation only transforms the colour plane.
    if transforms and config[CONF_TRANSPARENCY] == CONF_ALPHA_CHANNEL:
        raise cv.Invalid(
            "'crop' and 'rotation' cannot be combined with "
            "'transparency: alpha_channel'"
        )
    if (
        config[CONF_ROTATION] != 0
        and CONF_CROP not in config
        and CONF_RESIZE not in config
    ):
        raise cv.Invalid("'rotation' requires 'resize' or 'crop'")
    # A crop is checked against the size the frame actually decodes to in the
    # final validation, which needs the camera resolution; with `resize` that is
    # smaller than the resize box when the aspect ratios differ.
    return config


CONFIG_SCHEMA = cv.All(
    runtime_image.runtime_image_schema(CameraImage)
    .extend(
        {
            cv.Required(CONF_CAMERA_ID): cv.use_id(esp32_camera.ESP32Camera),
            # A camera only produces JPEG frames, so any other format could
            # never decode.
            cv.Required(CONF_FORMAT): cv.one_of("JPEG", "JPG", upper=True),
            cv.Optional(CONF_DOUBLE_BUFFER, default=False): cv.boolean,
            cv.Optional(CONF_CROP): _CROP_SCHEMA,
            cv.Optional(CONF_ROTATION, default=0): cv.one_of(0, 90, 180, 270),
            cv.Optional(CONF_ON_IMAGE): automation.validate_automation({}),
            cv.Optional(CONF_ON_ERROR): automation.validate_automation({}),
        }
    )
    .extend(cv.polling_component_schema("never")),
    _validate_options,
    runtime_image.validate_runtime_image_settings,
    cv.only_on_esp32,
)

_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(CONF_ON_IMAGE, "add_on_image_callback"),
    automation.CallbackAutomation(CONF_ON_ERROR, "add_on_error_callback"),
)


# The numeric resolution names in esp32_camera.FRAME_SIZES carry the pixel size;
# the aliases (QQVGA, VGA, ...) map to the same frame size.
def _resolution_dimensions(resolution: object) -> tuple[int, int]:
    name = str(resolution).upper()
    parts = name.split("X")
    if len(parts) == 2 and parts[0].isdigit() and parts[1].isdigit():
        return int(parts[0]), int(parts[1])

    try:
        frame_size = esp32_camera.FRAME_SIZES[name]
    except KeyError:
        raise cv.Invalid(f"Unknown camera resolution {resolution}") from None
    for numeric_name, candidate in esp32_camera.FRAME_SIZES.items():
        parts = numeric_name.split("X")
        # The enum values are codegen mock objects, one per attribute access, so
        # they are compared rendered rather than by identity.
        if len(parts) == 2 and parts[0].isdigit() and str(candidate) == str(frame_size):
            return int(parts[0]), int(parts[1])
    raise cv.Invalid(f"Unknown camera resolution {resolution}")


def _to_float32(value: float) -> float:
    """Round to float32, as RuntimeImage::resize() computes the scale in floats."""
    return unpack("f", pack("f", value))[0]


def _decoded_size(
    camera_width: int, camera_height: int, resize: tuple[int, int] | None
) -> tuple[int, int]:
    """Size the frame decodes to, matching RuntimeImage::resize()."""
    if resize is None:
        return camera_width, camera_height
    resize_width, resize_height = resize
    # Both dimensions are fixed, so the scale is the smaller ratio and the result
    # is truncated to whole pixels.
    scale = min(
        _to_float32(resize_width / camera_width),
        _to_float32(resize_height / camera_height),
    )
    return (
        int(_to_float32(camera_width * scale)),
        int(_to_float32(camera_height * scale)),
    )


def _final_validate(config: ConfigType) -> ConfigType:
    """Check the options that depend on the referenced camera's configuration."""
    camera_config = fv.get_declaring_config(config[CONF_CAMERA_ID])

    # Without jpeg_quality the camera returns raw pixels, which this platform can
    # never decode.
    pixel_format = camera_config.get(esp32_camera.CONF_PIXEL_FORMAT, "JPEG")
    if (
        pixel_format != "JPEG"
        and camera_config.get(esp32_camera.CONF_JPEG_QUALITY, 0) == 0
    ):
        raise cv.Invalid(
            f"camera_image needs JPEG frames, but the camera is configured with "
            f"pixel_format: {pixel_format} and jpeg_quality: 0, which delivers raw "
            f"frames. Set jpeg_quality so the camera converts to JPEG, or use "
            f"pixel_format: JPEG.",
            path=[CONF_CAMERA_ID],
        )

    # The crop has to fit the size the frame decodes to. With a resize that is
    # the resize box only when the aspect ratios match; otherwise the uniform
    # scale makes one side smaller.
    if CONF_CROP in config:
        camera_width, camera_height = _resolution_dimensions(
            camera_config[CONF_RESOLUTION]
        )
        decoded_width, decoded_height = _decoded_size(
            camera_width, camera_height, config.get(CONF_RESIZE)
        )
        crop = config[CONF_CROP]
        if (
            crop[CONF_X] + crop[CONF_WIDTH] > decoded_width
            or crop[CONF_Y] + crop[CONF_HEIGHT] > decoded_height
        ):
            if CONF_RESIZE in config:
                resize_width, resize_height = config[CONF_RESIZE]
                detail = (
                    f"the {decoded_width}x{decoded_height} frame decoded from the "
                    f"{camera_width}x{camera_height} camera with "
                    f"resize {resize_width}x{resize_height}"
                )
            else:
                detail = f"the {camera_width}x{camera_height} camera resolution"
            raise cv.Invalid(
                f"crop {crop[CONF_X]},{crop[CONF_Y]} "
                f"{crop[CONF_WIDTH]}x{crop[CONF_HEIGHT]} does not fit inside "
                f"{detail}",
                path=[CONF_CROP],
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_CAMERA_IMAGE")

    settings = await runtime_image.process_runtime_image_config(config)

    metadata_width = settings.width
    metadata_height = settings.height
    if CONF_CROP in config:
        metadata_width = config[CONF_CROP][CONF_WIDTH]
        metadata_height = config[CONF_CROP][CONF_HEIGHT]
    if config[CONF_ROTATION] in (90, 270):
        metadata_width, metadata_height = metadata_height, metadata_width

    add_metadata(
        config[CONF_ID],
        metadata_width,
        metadata_height,
        config[CONF_TYPE],
        config[CONF_TRANSPARENCY],
    )

    camera_var = await cg.get_variable(config[CONF_CAMERA_ID])
    var = cg.new_Pvariable(
        config[CONF_ID],
        camera_var,
        settings.width,
        settings.height,
        settings.format_enum,
        settings.image_type_enum,
        settings.transparent,
        settings.placeholder or cg.nullptr,
        settings.byte_order_big_endian,
    )
    await cg.register_component(var, config)

    if settings.double_buffer:
        cg.add(var.set_double_buffer(True))

    if CONF_CROP in config:
        crop = config[CONF_CROP]
        cg.add(
            var.set_crop(
                crop[CONF_X], crop[CONF_Y], crop[CONF_WIDTH], crop[CONF_HEIGHT]
            )
        )
    if config[CONF_ROTATION]:
        cg.add(var.set_rotation(config[CONF_ROTATION]))
    if CONF_CROP in config or config[CONF_ROTATION] != 0:
        cg.add(var.set_transform_size(metadata_width, metadata_height))

    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)
