# __init__.py
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_FLIP_X,
    CONF_FLIP_Y,
    CONF_HEIGHT,
    CONF_ID,
    CONF_TYPE,
    CONF_WIDTH,
)

CODEOWNERS = ["@DT-art1", "@nliaudat"]

AUTO_LOAD = ["camera"]

CONF_CAMERA_ID = "camera_id"
CONF_CROP_X = "crop_x"
CONF_CROP_Y = "crop_y"

CONF_IMAGE_ID = "image_id"
CONF_IMAGE_FORMAT = "image_format"
CONF_IMAGE_FORMAT_ID = "image_format_id"

DEFAULT_CROPPER = "default"

camera_ns = cg.esphome_ns.namespace("camera")
camera_cropper_ns = cg.esphome_ns.namespace("camera_cropper")

Processor = camera_ns.class_("Processor")
Camera = camera_ns.class_("CameraImpl")
BufferImpl = camera_ns.class_("BufferImpl")
CameraCropper = camera_cropper_ns.class_("CameraCropper", Processor)

CameraImageSpec = camera_ns.struct("CameraImageSpec")
ImageFormat = camera_ns.enum("ImageFormat")

CONF_IMAGE_FORMAT_SELECTS = {
    "GRAYSCALE": ImageFormat.IMAGE_FORMAT_GRAYSCALE,
    "RGB565": ImageFormat.IMAGE_FORMAT_RGB565,
    "BGR888": ImageFormat.IMAGE_FORMAT_BGR888,
}

BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CAMERA_ID): cv.use_id(Camera),
        cv.Required(CONF_HEIGHT): cv.int_range(min=1),
        cv.Required(CONF_WIDTH): cv.int_range(min=1),
        cv.Required(CONF_IMAGE_FORMAT): cv.enum(CONF_IMAGE_FORMAT_SELECTS, upper=True),
    }
)

DEFAULT_CROPPER_SCHEMA = BASE_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CameraCropper),
            cv.Required(CONF_CROP_X): cv.int_range(min=0),
            cv.Required(CONF_CROP_Y): cv.int_range(min=0),
            cv.Optional(CONF_FLIP_X, default=False): cv.boolean,
            cv.Optional(CONF_FLIP_Y, default=False): cv.boolean,
            cv.GenerateID(CONF_IMAGE_ID): cv.declare_id(BufferImpl),
            cv.GenerateID(CONF_IMAGE_FORMAT_ID): cv.declare_id(CameraImageSpec),
        }
    )
)

CONFIG_SCHEMA = cv.ensure_list(
    cv.typed_schema(
        {
            DEFAULT_CROPPER: DEFAULT_CROPPER_SCHEMA,
        },
        default_type=DEFAULT_CROPPER,
    )
)


async def to_code(config):
    for conf in config:
        if conf[CONF_TYPE] == DEFAULT_CROPPER:
            # Create spec with the correct format (even though it will be overridden)
            spec = cg.new_Pvariable(
                conf[CONF_IMAGE_FORMAT_ID],
                cg.StructInitializer(
                    CameraImageSpec,
                    ("width", conf[CONF_WIDTH]),
                    ("height", conf[CONF_HEIGHT]),
                    ("format", conf[CONF_IMAGE_FORMAT]),
                ),
            )

            # Create image and set data length
            image = cg.new_Pvariable(conf[CONF_IMAGE_ID], spec)

            # Create cropper with all required parameters
            cropper = cg.new_Pvariable(
                conf[CONF_ID],
                spec,
                image,
                conf[CONF_CROP_X],
                conf[CONF_CROP_Y],
                conf[CONF_WIDTH],
                conf[CONF_HEIGHT],
            )

            # Set flip options if specified
            if conf[CONF_FLIP_X]:
                cg.add(cropper.set_flip_x(conf[CONF_FLIP_X]))
            if conf[CONF_FLIP_Y]:
                cg.add(cropper.set_flip_y(conf[CONF_FLIP_Y]))

            # Add processor to camera
            camera = await cg.get_variable(conf[CONF_CAMERA_ID])
            cg.add(camera.append_processor(cropper))
