from pathlib import Path

from esphome import automation
from esphome.automation import Trigger
import esphome.codegen as cg
from esphome.components.camera import CONF_PIPELINE_ID, CONF_TASK_ID, Pipeline, Task
from esphome.components.camera_sensor import CONF_CAMERA_SENSOR_ID
from esphome.components.esp32 import VARIANT_ESP32P4, add_idf_component, only_on_variant
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_CLEAR,
    CONF_FILE,
    CONF_FLIP_X,
    CONF_FLIP_Y,
    CONF_FORMAT,
    CONF_HEIGHT,
    CONF_ID,
    CONF_MODE,
    CONF_RAW_DATA_ID,
    CONF_ROTATION,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    CONF_WIDTH,
    CONF_X,
    CONF_Y,
)
from esphome.core import EsphomeError, HexInt, Lambda
from esphome.cpp_generator import ExpressionStatement, MockObj

CODEOWNERS = ["@DT-art1"]
AUTO_LOAD = ["camera", "display"]
MULTI_CONF = True

CONF_ALGORITHM = "algorithm"
CONF_BUFFERS = "buffers"
CONF_COPY = "copy"
CONF_CROP_X = "crop_x"
CONF_CROP_Y = "crop_y"
CONF_CAMERA_ENCODER_ID = "camera_encoder_id"
CONF_FORMAT_ID = "format_id"
CONF_IMAGE_ID = "image_id"
CONF_NEXT = "next"
CONF_RETRIES = "retries"
CONF_REQUESTER = "requester"
CONF_RUN_AS_JOB = "run_as_job"
CONF_SCALER_ID = "scaler_id"
CONF_SCALE_X = "scale_x"
CONF_SCALE_Y = "scale_y"
CONF_STATISTICS = "statistics"

CONF_ON_PRE_PROCESS = "on_pre_process"
CONF_ON_POST_PROCESS = "on_post_process"

ACCELERATE = "accelerated_scale"
CONVERT = "convert"
CROP = "crop"
INFER = "infer"
INPUT = "input"
OUTPUT = "output"
OVERLAY = "overlay"
PRESENT = "present"
ROTATE = "rotate"
SCALE = "scale"

MAX_OUTPUT_BUFFER_SIZE_2MB = 2 * 1024 * 1024
DEFAULT_OUTPUT_BUFFER_SIZE = 1920 * 1080 / 10

camera_ns = cg.esphome_ns.namespace("camera")
camera_pipeline_ns = cg.esphome_ns.namespace("camera_pipeline")

BufferImpl = camera_ns.class_("BufferImpl")
Camera = camera_ns.class_("CameraImpl")
CameraImageSpec = camera_ns.struct("CameraImageSpec")
Reentry = camera_ns.class_("Reentry")
Encoder = camera_ns.class_("Encoder")
Resolution = camera_ns.struct("Resolution")
Sensor = camera_ns.class_("Sensor")

ProcessorBase = camera_pipeline_ns.class_("ProcessorBase")

AcceleratorMode = camera_pipeline_ns.enum("AcceleratorMode")
AcceleratorRotation = camera_pipeline_ns.enum("AcceleratorRotation")
PixelFormat = camera_ns.enum("PixelFormat")
CameraRequester = camera_ns.enum("CameraRequester")
ScalerAlgorithm = camera_pipeline_ns.enum("ScalerAlgorithm")
InferencerMode = camera_pipeline_ns.enum("InferencerMode")

Accelerator = camera_pipeline_ns.class_("Accelerator", ProcessorBase)
Converter = camera_pipeline_ns.class_("Converter", ProcessorBase)
Cropper = camera_pipeline_ns.class_("Cropper", ProcessorBase)
Inferencer = camera_pipeline_ns.class_("Inferencer", ProcessorBase)
Inputer = camera_pipeline_ns.class_("Inputer", ProcessorBase)
Outputer = camera_pipeline_ns.class_("Outputer", ProcessorBase)
Overlayer = camera_pipeline_ns.class_("Overlayer", ProcessorBase)
Presenter = camera_pipeline_ns.class_("Presenter", ProcessorBase)
Rotator = camera_pipeline_ns.class_("Rotator", ProcessorBase)
Scaler = camera_pipeline_ns.class_("Scaler", ProcessorBase)

ReentryRef = Reentry.operator("ref")

CONF_ACCELERATOR_MODE_SELECTS = {
    "SCALE_ROTATE_MIRROR": AcceleratorMode.ACCELERATOR_SCALE_ROTATE_MIRROR,
    "BLEND": AcceleratorMode.ACCELERATOR_BLEND,
    "FILL": AcceleratorMode.ACCELERATOR_FILL,
}

CONF_ACCELERATOR_ROTATION_SELECTS = {
    "0": AcceleratorRotation.ACCELERATOR_ANGLE_0,
    "90": AcceleratorRotation.ACCELERATOR_ANGLE_90,
    "180": AcceleratorRotation.ACCELERATOR_ANGLE_180,
    "270": AcceleratorRotation.ACCELERATOR_ANGLE_270,
}

CONF_FORMAT_SELECTS = {
    "GRAYSCALE": PixelFormat.PIXEL_FORMAT_GRAYSCALE,
    "RGB565": PixelFormat.PIXEL_FORMAT_RGB565,
    "BGR888": PixelFormat.PIXEL_FORMAT_BGR888,
}

CONF_ACCELERATED_FORMAT_SELECTS = {
    "RGB565": PixelFormat.PIXEL_FORMAT_RGB565,
    "BGR888": PixelFormat.PIXEL_FORMAT_BGR888,
}

CONF_REQUESTER_SELECTS = {
    "ALL": -1,
    "IDLE": CameraRequester.IDLE,
    "API": CameraRequester.API_REQUESTER,
    "WEB": CameraRequester.WEB_REQUESTER,
}

CONF_SCALER_ALGORITHM_SELECTS = {
    "DOWN_RESAMPLE": ScalerAlgorithm.DOWN_RESAMPLE,
    "BILINEAR": ScalerAlgorithm.BILINEAR,
}

CONF_INFERENCER_MODE_SELECTS = {
    "AUTO": InferencerMode.AUTO,
    "SINGLE_CORE": InferencerMode.SINGLE_CORE,
    "MULTI_CORE": InferencerMode.MULTI_CORE,
}

BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TASK_ID): cv.use_id(Task),
        cv.GenerateID(CONF_PIPELINE_ID): cv.use_id(Pipeline),
        cv.Optional(CONF_RUN_AS_JOB, default=False): cv.boolean,
        cv.Optional(CONF_NEXT, default=[]): cv.ensure_list(cv.use_id(ProcessorBase)),
        cv.Optional(CONF_STATISTICS, default=False): cv.boolean,
    }
)


def BASE_WITH_AUTOMATION(Type):
    return BASE_SCHEMA.extend(
        cv.Schema(
            {
                cv.Optional(CONF_ON_PRE_PROCESS): automation.validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                            Trigger.template(Type.operator("ref"), ReentryRef)
                        ),
                    }
                ),
                cv.Optional(CONF_ON_POST_PROCESS): automation.validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                            Trigger.template(Type.operator("ref"), ReentryRef)
                        ),
                    }
                ),
            }
        )
    )


def validate_accelerator_scale(value):
    step = 1.0 / 16.0
    steps = round(value / step)
    if abs(value - steps * step) > 1e-6:
        raise cv.Invalid(f"scale must be multiple of 1/16 (0.0625). Got {value}")
    return value


ACCELERATOR_SCHEMA = cv.All(
    BASE_WITH_AUTOMATION(Accelerator).extend(
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(Accelerator),
                cv.Optional(CONF_MODE, default="SCALE_ROTATE_MIRROR"): cv.enum(
                    CONF_ACCELERATOR_MODE_SELECTS, upper=True
                ),
                cv.Optional(CONF_FORMAT): cv.enum(
                    CONF_ACCELERATED_FORMAT_SELECTS, upper=True
                ),
                cv.Optional(CONF_SCALE_X, default=1.0): cv.All(
                    cv.float_range(0.0625, 100.0), validate_accelerator_scale
                ),
                cv.Optional(CONF_SCALE_Y, default=1.0): cv.float_range(0.0625, 100.0),
                cv.Optional(CONF_FLIP_X, default=False): cv.boolean,
                cv.Optional(CONF_FLIP_Y, default=False): cv.boolean,
                cv.Optional(CONF_ROTATION, default="0"): cv.enum(
                    CONF_ACCELERATOR_ROTATION_SELECTS, upper=True
                ),
                cv.GenerateID(CONF_IMAGE_ID): cv.declare_id(BufferImpl),
            }
        ),
    ),
    only_on_variant(supported=[VARIANT_ESP32P4]),
)

CONVERTER_SCHEMA = BASE_WITH_AUTOMATION(Converter).extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Converter),
            cv.Required(CONF_FORMAT): cv.enum(CONF_FORMAT_SELECTS, upper=True),
            cv.GenerateID(CONF_IMAGE_ID): cv.declare_id(BufferImpl),
        }
    )
)

CROPPER_SCHEMA = BASE_WITH_AUTOMATION(Cropper).extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Cropper),
            cv.Required(CONF_HEIGHT): cv.int_range(0, 1080),
            cv.Required(CONF_WIDTH): cv.int_range(0, 1920),
            cv.Required(CONF_CROP_X): cv.int_range(0, 1920),
            cv.Required(CONF_CROP_Y): cv.int_range(0, 1080),
            cv.GenerateID(CONF_IMAGE_ID): cv.declare_id(BufferImpl),
        }
    )
)

INFERENCER_SCHEMA = BASE_WITH_AUTOMATION(Inferencer).extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Inferencer),
            cv.Required(CONF_FILE): cv.file_,
            cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
            cv.Optional(CONF_MODE, default="SINGLE_CORE"): cv.enum(
                CONF_INFERENCER_MODE_SELECTS, upper=True
            ),
        }
    )
)

INPUTER_SCHEMA = BASE_WITH_AUTOMATION(Inputer).extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Inputer),
            cv.GenerateID(CONF_CAMERA_SENSOR_ID): cv.use_id(Sensor),
            cv.Optional(CONF_RETRIES, default=100): cv.Any(0, 100),
        }
    )
)

OVERLAYER_SCHEMA = BASE_WITH_AUTOMATION(Overlayer).extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Overlayer),
            cv.Optional(CONF_COPY, default=False): cv.boolean,
            cv.GenerateID(CONF_IMAGE_ID): cv.declare_id(BufferImpl),
        }
    )
)

OUTPUTER_SCHEMA = BASE_WITH_AUTOMATION(Outputer).extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Outputer),
            cv.Optional(CONF_CAMERA_ENCODER_ID): cv.use_id(Encoder),
            cv.Optional(CONF_BUFFERS, default=2): cv.int_range(1, 2),
            cv.Optional(
                CONF_BUFFER_SIZE, default=DEFAULT_OUTPUT_BUFFER_SIZE
            ): cv.int_range(1024, MAX_OUTPUT_BUFFER_SIZE_2MB),
            cv.Optional(CONF_NEXT): cv.none,
            cv.Optional(CONF_REQUESTER, default="ALL"): cv.enum(
                CONF_REQUESTER_SELECTS, upper=True
            ),
        }
    )
)

PRESENTER_SCHEMA = BASE_WITH_AUTOMATION(Presenter).extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Presenter),
            cv.Required(CONF_HEIGHT): cv.int_range(0, 1080),
            cv.Required(CONF_WIDTH): cv.int_range(0, 1920),
            cv.Required(CONF_FORMAT): cv.enum(CONF_FORMAT_SELECTS, upper=True),
            cv.Required(CONF_X): cv.int_range(0, 1920),
            cv.Required(CONF_Y): cv.int_range(0, 1080),
            cv.GenerateID(CONF_IMAGE_ID): cv.declare_id(BufferImpl),
            cv.GenerateID(CONF_FORMAT_ID): cv.declare_id(CameraImageSpec),
        }
    )
)

ROTATOR_SCHEMA = BASE_WITH_AUTOMATION(Rotator).extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Rotator),
            cv.Required(CONF_ROTATION): cv.int_range(0, 360),
            cv.Optional(CONF_CLEAR, default=True): cv.boolean,
            cv.GenerateID(CONF_IMAGE_ID): cv.declare_id(BufferImpl),
        }
    )
)

SCALER_SCHEMA = BASE_WITH_AUTOMATION(Scaler).extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Scaler),
            cv.Required(CONF_HEIGHT): cv.int_range(0, 1080),
            cv.Required(CONF_WIDTH): cv.int_range(0, 1920),
            cv.Optional(CONF_ALGORITHM, default="DOWN_RESAMPLE"): cv.enum(
                CONF_SCALER_ALGORITHM_SELECTS, upper=True
            ),
            cv.GenerateID(CONF_IMAGE_ID): cv.declare_id(BufferImpl),
        }
    )
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        ACCELERATE: ACCELERATOR_SCHEMA,
        CONVERT: CONVERTER_SCHEMA,
        CROP: CROPPER_SCHEMA,
        INFER: INFERENCER_SCHEMA,
        INPUT: INPUTER_SCHEMA,
        OUTPUT: OUTPUTER_SCHEMA,
        OVERLAY: OVERLAYER_SCHEMA,
        PRESENT: PRESENTER_SCHEMA,
        ROTATE: ROTATOR_SCHEMA,
        SCALE: SCALER_SCHEMA,
    },
)


async def setup_automation_trigger(var, conf, type):
    trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
    trigger = await automation.build_automation(
        trigger, [(type.operator("ref"), "processor"), (ReentryRef, "reentry")], conf
    )
    return await cg.process_lambda(
        Lambda(
            str(
                ExpressionStatement(
                    trigger.trigger(MockObj("processor"), MockObj("reentry"))
                )
            )
        ),
        [(type.operator("ref"), "processor"), (ReentryRef, "reentry")],
    )


async def setup_automation(var, config, type):
    for conf in config.get(CONF_ON_PRE_PROCESS, []):
        trigger = await setup_automation_trigger(var, conf, type)
        cg.add(var.add_pre_process_callback(trigger))
    for conf in config.get(CONF_ON_POST_PROCESS, []):
        trigger = await setup_automation_trigger(var, conf, type)
        cg.add(var.add_post_process_callback(trigger))


async def to_code(config):
    pipeline = await cg.get_variable(config[CONF_PIPELINE_ID])
    if config[CONF_TYPE] == ACCELERATE:
        image = cg.new_Pvariable(config[CONF_IMAGE_ID])
        var = cg.new_Pvariable(config[CONF_ID], image)
        cg.add(var.set_mode(config[CONF_MODE]))
        cg.add(var.set_scale_x(config[CONF_SCALE_X]))
        cg.add(var.set_scale_y(config[CONF_SCALE_Y]))
        cg.add(var.set_flip_x(config[CONF_FLIP_X]))
        cg.add(var.set_flip_y(config[CONF_FLIP_Y]))
        cg.add(var.set_rotation(config[CONF_ROTATION]))
        if CONF_FORMAT in config:
            cg.add(var.set_format(config[CONF_FORMAT]))
        await setup_automation(var, config, Accelerator)
    if config[CONF_TYPE] == CONVERT:
        add_idf_component(name="espressif/esp_image_effects", ref="1.0.1")
        cg.add_build_flag("-DUSE_CAMERA_CONVERTER")
        image = cg.new_Pvariable(config[CONF_IMAGE_ID])
        var = cg.new_Pvariable(config[CONF_ID], config[CONF_FORMAT], image)
        await setup_automation(var, config, Converter)
    if config[CONF_TYPE] == CROP:
        add_idf_component(name="espressif/esp_image_effects", ref="1.0.1")
        cg.add_build_flag("-DUSE_CAMERA_CROPPER")
        image = cg.new_Pvariable(config[CONF_IMAGE_ID])
        var = cg.new_Pvariable(
            config[CONF_ID],
            config[CONF_WIDTH],
            config[CONF_HEIGHT],
            image,
            config[CONF_CROP_X],
            config[CONF_CROP_Y],
        )
        await setup_automation(var, config, Cropper)
    if config[CONF_TYPE] == INFER:
        add_idf_component(name="espressif/esp-dl", ref="3.2.0")
        cg.add_build_flag("-DUSE_CAMERA_INFERENCER")
        path = Path(config[CONF_FILE])
        if not path.is_file():
            raise EsphomeError(f"Could not load model file {path}")
        with open(path, "rb") as f:
            data = f.read()
        rhs = [HexInt(x) for x in data]
        prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)

        var = cg.new_Pvariable(
            config[CONF_ID],
            prog_arr,
        )
        cg.add(var.set_mode(config[CONF_MODE]))
        await setup_automation(var, config, Inferencer)
    if config[CONF_TYPE] == INPUT:
        cg.add_build_flag("-DUSE_CAMERA_INPUTER")
        sensor = await cg.get_variable(config[CONF_CAMERA_SENSOR_ID])
        var = cg.new_Pvariable(config[CONF_ID], sensor)
        cg.add(var.set_retry_limit(config[CONF_RETRIES]))
        await setup_automation(var, config, Inputer)
    if config[CONF_TYPE] == OUTPUT:
        cg.add_build_flag("-DUSE_CAMERA_OUTPUTER")
        var = cg.new_Pvariable(
            config[CONF_ID],
        )
        cg.add(var.set_buffers(config[CONF_BUFFERS]))
        cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))
        if CONF_CAMERA_ENCODER_ID in config:
            encoder = await cg.get_variable(config[CONF_CAMERA_ENCODER_ID])
            cg.add(var.set_encoder(encoder))
        if config[CONF_REQUESTER] == "ALL":
            cg.add(var.set_requester_all())
            cg.add(pipeline.set_default_output(var))
        else:
            cg.add(var.set_requester(config[CONF_REQUESTER]))
            cg.add(pipeline.add_output(var, config[CONF_REQUESTER]))
        await setup_automation(var, config, Outputer)
    if config[CONF_TYPE] == OVERLAY:
        cg.add_build_flag("-DUSE_CAMERA_OVERLAYER")
        var = cg.new_Pvariable(
            config[CONF_ID],
        )
        if config[CONF_COPY]:
            image = cg.new_Pvariable(config[CONF_IMAGE_ID])
            cg.add(var.set_copy_buffer(image))
        await setup_automation(var, config, Overlayer)
    if config[CONF_TYPE] == PRESENT:
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
        var = cg.new_Pvariable(
            config[CONF_ID],
            cg.StructInitializer(
                CameraImageSpec,
                ("width", config[CONF_WIDTH]),
                ("height", config[CONF_HEIGHT]),
                ("format", config[CONF_FORMAT]),
            ),
            image,
            config[CONF_X],
            config[CONF_Y],
        )
        await setup_automation(var, config, Presenter)
    if config[CONF_TYPE] == ROTATE:
        add_idf_component(name="espressif/esp_image_effects", ref="1.0.1")
        cg.add_build_flag("-DUSE_CAMERA_ROTATOR")
        image = cg.new_Pvariable(config[CONF_IMAGE_ID])
        var = cg.new_Pvariable(config[CONF_ID], image, config[CONF_ROTATION])
        cg.add(var.set_clear(config[CONF_CLEAR]))
        await setup_automation(var, config, Rotator)
    if config[CONF_TYPE] == SCALE:
        add_idf_component(name="espressif/esp_image_effects", ref="1.0.1")
        cg.add_build_flag("-DUSE_CAMERA_SCALER")
        image = cg.new_Pvariable(config[CONF_IMAGE_ID])
        var = cg.new_Pvariable(
            config[CONF_ID],
            config[CONF_ALGORITHM],
            config[CONF_WIDTH],
            config[CONF_HEIGHT],
            image,
        )
        await setup_automation(var, config, Scaler)

    cg.add(var.set_id(str(config[CONF_ID])))
    cg.add(var.set_statistics(config[CONF_STATISTICS]))
    cg.add(pipeline.add_processor(var))
    if CONF_NEXT in config:
        for next in config[CONF_NEXT]:
            child = await cg.get_variable(next)
            cg.add(pipeline.add_link(var, child))

    if config[CONF_TYPE] == INPUT:
        cg.add(pipeline.set_input(var))

    task = await cg.get_variable(config[CONF_TASK_ID])
    cg.add(var.set_task(task))
    cg.add(var.set_run_as_job(config[CONF_RUN_AS_JOB]))
