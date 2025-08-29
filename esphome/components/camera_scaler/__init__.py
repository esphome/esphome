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

CODEOWNERS = ["@DT-art1"]

AUTO_LOAD = ["camera"]

CONF_ALGORITHM = "algorithm"
CONF_CAMERA_ID = "camera_id"
CONF_CLEAR = "clear"
CONF_DEFAULT_SCALER_ID = "default_scaler_id"
CONF_IMAGE_FORMAT = "image_format"
CONF_IMAGE_FORMAT_ID = "image_format_id"
CONF_IMAGE_ID = "image_id"
CONF_MARGINS = "margins"
CONF_LEFT = "left"
CONF_TOP = "top"
CONF_RIGHT = "right"
CONF_BOTTOM = "bottom"

DEFAULT_SCALER = "default"

camera_ns = cg.esphome_ns.namespace("camera")
camera_scaler_ns = cg.esphome_ns.namespace("camera_scaler")

Processor = camera_ns.class_("Processor")
Camera = camera_ns.class_("CameraImpl")
BufferImpl = camera_ns.class_("BufferImpl")
DefaultScaler = camera_scaler_ns.class_("DefaultScaler", Processor)

CameraImageSpec = camera_ns.struct("CameraImageSpec")

ImageFormat = camera_ns.enum("ImageFormat")
DefaultAlgorithm = camera_scaler_ns.enum("DefaultAlgorithm")

CONF_IMAGE_FORMAT_SELECTS = {
    "GRAYSCALE": ImageFormat.IMAGE_FORMAT_GRAYSCALE,
}

CONF_ALGORITHM_SELECTS = {
    "NEAREST_NEIGHBOR": DefaultAlgorithm.NEAREST_NEIGHBOR,
    "BILINEAR": DefaultAlgorithm.BILINEAR,
}

margin_parameters = {
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
        cv.Required(CONF_IMAGE_FORMAT): cv.enum(CONF_IMAGE_FORMAT_SELECTS, upper=True),
    }
)

DEFAULT_SCALER_SCHEMA = BASE_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DefaultScaler),
            cv.Required(CONF_ALGORITHM): cv.enum(CONF_ALGORITHM_SELECTS, upper=True),
            cv.Optional(CONF_FLIP_X, default=False): cv.boolean,
            cv.Optional(CONF_FLIP_Y, default=False): cv.boolean,
            cv.Optional(CONF_MARGINS): cv.ensure_list(margin_parameters),
            cv.Optional(CONF_CLEAR, default=False): cv.boolean,
            cv.GenerateID(CONF_IMAGE_ID): cv.declare_id(BufferImpl),
            cv.GenerateID(CONF_IMAGE_FORMAT_ID): cv.declare_id(CameraImageSpec),
        }
    )
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        DEFAULT_SCALER: DEFAULT_SCALER_SCHEMA,
    },
    default_type=DEFAULT_SCALER,
)


async def to_code(config):
    if config[CONF_TYPE] == DEFAULT_SCALER:
        spec = cg.new_Pvariable(
            config[CONF_IMAGE_FORMAT_ID],
            cg.StructInitializer(
                CameraImageSpec,
                ("width", config[CONF_WIDTH]),
                ("height", config[CONF_HEIGHT]),
                ("format", config[CONF_IMAGE_FORMAT]),
            ),
        )
        image = cg.new_Pvariable(config[CONF_IMAGE_ID], spec)
        scaler = cg.new_Pvariable(config[CONF_ID], config[CONF_ALGORITHM], spec, image)
        if config[CONF_FLIP_X]:
            cg.add(scaler.set_flip_x(config[CONF_FLIP_X]))
        if config[CONF_FLIP_Y]:
            cg.add(scaler.set_flip_y(config[CONF_FLIP_Y]))
        if config[CONF_CLEAR]:
            cg.add(scaler.set_clear(config[CONF_CLEAR]))
        if CONF_MARGINS in config:
            for margin in config[CONF_MARGINS]:
                if CONF_LEFT in margin:
                    cg.add(scaler.set_margin_left(margin[CONF_LEFT]))
                if CONF_RIGHT in margin:
                    cg.add(scaler.set_margin_right(margin[CONF_RIGHT]))
                if CONF_TOP in margin:
                    cg.add(scaler.set_margin_top(margin[CONF_TOP]))
                if CONF_BOTTOM in margin:
                    cg.add(scaler.set_margin_bottom(margin[CONF_BOTTOM]))

        camera = await cg.get_variable(config[CONF_CAMERA_ID])
        cg.add(camera.append_processor(scaler))
