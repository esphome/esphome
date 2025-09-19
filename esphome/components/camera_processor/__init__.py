from esphome import automation
from esphome.automation import Trigger
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLEAR,
    CONF_FLIP_X,
    CONF_FLIP_Y,
    CONF_FORMAT,
    CONF_HEIGHT,
    CONF_ID,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    CONF_WIDTH,
)
from esphome.core import Lambda
from esphome.cpp_generator import ExpressionStatement, MockObj

CODEOWNERS = ["@DT-art1", "@nliaudat"]
AUTO_LOAD = ["camera"]
MULTI_CONF = True

CONF_ALGORITHM = "algorithm"
CONF_CAMERA_ID = "camera_id"
CONF_CROP_X = "crop_x"
CONF_CROP_Y = "crop_y"
CONF_SCALER_ID = "scaler_id"
CONF_FORMAT_ID = "format_id"
CONF_IMAGE_ID = "image_id"
CONF_MARGINS = "margins"
CONF_LEFT = "left"
CONF_TOP = "top"
CONF_RIGHT = "right"
CONF_BOTTOM = "bottom"

CONF_ON_PROCESS = "on_process"

SCALER = "scaler"
CROPPER = "cropper"

camera_ns = cg.esphome_ns.namespace("camera")
camera_processor_ns = cg.esphome_ns.namespace("camera_processor")

Buffer = camera_ns.class_("Buffer")
BufferImpl = camera_ns.class_("BufferImpl")
Camera = camera_ns.class_("CameraImpl")
CameraImageSpec = camera_ns.struct("CameraImageSpec")

ProcessorBase = camera_processor_ns.class_("ProcessorBase")

BufferPtr = Buffer.operator("ptr")
CameraImageSpecPtr = CameraImageSpec.operator("ptr")

PixelFormat = camera_ns.enum("PixelFormat")
ScalerAlgorithm = camera_processor_ns.enum("ScalerAlgorithm")

Cropper = camera_processor_ns.class_("Cropper", ProcessorBase)
Scaler = camera_processor_ns.class_("Scaler", ProcessorBase)

CONF_FORMAT_SELECTS = {
    "GRAYSCALE": PixelFormat.PIXEL_FORMAT_GRAYSCALE,
    "RGB565": PixelFormat.PIXEL_FORMAT_RGB565,
    "BGR888": PixelFormat.PIXEL_FORMAT_BGR888,
}

CONF_SCALER_ALGORITHM_SELECTS = {
    "NEAREST_NEIGHBOR": ScalerAlgorithm.NEAREST_NEIGHBOR,
    "BILINEAR": ScalerAlgorithm.BILINEAR,
}

scaler_margin_parameters = {
    cv.Optional(CONF_LEFT): cv.int_range(0),
    cv.Optional(CONF_TOP): cv.int_range(0),
    cv.Optional(CONF_RIGHT): cv.int_range(0),
    cv.Optional(CONF_BOTTOM): cv.int_range(0),
}

BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_CAMERA_ID): cv.use_id(Camera),
        cv.Required(CONF_HEIGHT): cv.int_range(0),
        cv.Required(CONF_WIDTH): cv.int_range(0),
        cv.Required(CONF_FORMAT): cv.enum(CONF_FORMAT_SELECTS, upper=True),
        cv.Optional(CONF_ON_PROCESS): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    Trigger.template(CameraImageSpecPtr, BufferPtr)
                ),
            }
        ),
    }
)

SCALER_SCHEMA = BASE_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Scaler),
            cv.Required(CONF_ALGORITHM): cv.enum(
                CONF_SCALER_ALGORITHM_SELECTS, upper=True
            ),
            cv.Optional(CONF_FLIP_X, default=False): cv.boolean,
            cv.Optional(CONF_FLIP_Y, default=False): cv.boolean,
            cv.Optional(CONF_MARGINS): cv.ensure_list(scaler_margin_parameters),
            cv.Optional(CONF_CLEAR, default=False): cv.boolean,
            cv.GenerateID(CONF_IMAGE_ID): cv.declare_id(BufferImpl),
            cv.GenerateID(CONF_FORMAT_ID): cv.declare_id(CameraImageSpec),
        }
    )
)

CROPPER_SCHEMA = BASE_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Cropper),
            cv.Required(CONF_CROP_X): cv.int_range(min=0),
            cv.Required(CONF_CROP_Y): cv.int_range(min=0),
            cv.Optional(CONF_FLIP_X, default=False): cv.boolean,
            cv.Optional(CONF_FLIP_Y, default=False): cv.boolean,
            cv.GenerateID(CONF_IMAGE_ID): cv.declare_id(BufferImpl),
            cv.GenerateID(CONF_FORMAT_ID): cv.declare_id(CameraImageSpec),
        }
    )
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        CROPPER: CROPPER_SCHEMA,
        SCALER: SCALER_SCHEMA,
    },
)


async def to_code(config):
    if config[CONF_TYPE] == CROPPER:
        # Create spec with the correct format (even though it will be overridden)
        spec = cg.new_Pvariable(
            config[CONF_FORMAT_ID],
            cg.StructInitializer(
                CameraImageSpec,
                ("width", config[CONF_WIDTH]),
                ("height", config[CONF_HEIGHT]),
                ("format", config[CONF_FORMAT]),
            ),
        )

        # Create image and set data length
        image = cg.new_Pvariable(config[CONF_IMAGE_ID], spec)

        # Create cropper with all required parameters
        var = cg.new_Pvariable(
            config[CONF_ID],
            spec,
            image,
            config[CONF_CROP_X],
            config[CONF_CROP_Y],
            config[CONF_WIDTH],
            config[CONF_HEIGHT],
        )

        # Set flip options if specified
        if config[CONF_FLIP_X]:
            cg.add(var.set_flip_x(config[CONF_FLIP_X]))
        if config[CONF_FLIP_Y]:
            cg.add(var.set_flip_y(config[CONF_FLIP_Y]))

    if config[CONF_TYPE] == SCALER:
        spec = cg.new_Pvariable(
            config[CONF_FORMAT_ID],
            cg.StructInitializer(
                CameraImageSpec,
                ("width", config[CONF_WIDTH]),
                ("height", config[CONF_HEIGHT]),
                ("format", config[CONF_FORMAT]),
            ),
        )
        image = cg.new_Pvariable(config[CONF_IMAGE_ID], spec)
        var = cg.new_Pvariable(config[CONF_ID], config[CONF_ALGORITHM], spec, image)
        if config[CONF_FLIP_X]:
            cg.add(var.set_flip_x(config[CONF_FLIP_X]))
        if config[CONF_FLIP_Y]:
            cg.add(var.set_flip_y(config[CONF_FLIP_Y]))
        if config[CONF_CLEAR]:
            cg.add(var.set_clear(config[CONF_CLEAR]))
        if CONF_MARGINS in config:
            for margin in config[CONF_MARGINS]:
                if CONF_LEFT in margin:
                    cg.add(var.set_margin_left(margin[CONF_LEFT]))
                if CONF_RIGHT in margin:
                    cg.add(var.set_margin_right(margin[CONF_RIGHT]))
                if CONF_TOP in margin:
                    cg.add(var.set_margin_top(margin[CONF_TOP]))
                if CONF_BOTTOM in margin:
                    cg.add(var.set_margin_bottom(margin[CONF_BOTTOM]))

    camera = await cg.get_variable(config[CONF_CAMERA_ID])
    cg.add(camera.append_processor(var))

    for conf in config.get(CONF_ON_PROCESS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        trigger = await automation.build_automation(
            trigger, [(CameraImageSpecPtr, "spec"), (BufferPtr, "buffer")], conf
        )
        trigger = Lambda(
            str(
                ExpressionStatement(trigger.trigger(MockObj("spec"), MockObj("buffer")))
            )
        )
        trigger = await cg.process_lambda(
            trigger, [(CameraImageSpecPtr, "spec"), (BufferPtr, "buffer")]
        )
        cg.add(var.add_process_callback(trigger))
