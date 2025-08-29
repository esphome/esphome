from esphome import automation
import esphome.codegen as cg
from esphome.components import camera_encoder, camera_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TRIGGER_ID
from esphome.core import coroutine_with_priority
from esphome.core.entity_helpers import setup_entity
from esphome.cpp_generator import MockObjClass

CODEOWNERS = ["@DT-art1", "@bdraco"]

CONF_IDLE_UPDATE_INTERVAL = "idle_update_interval"
CONF_MAX_UPDATE_INTERVAL = "max_update_interval"
CONF_CAMERA_ENCODER_ID = "camera_encoder_id"
CONF_CAMERA_SENSOR_ID = "camera_sensor_id"

CONF_ON_STREAM_START = "on_stream_start"
CONF_ON_STREAM_STOP = "on_stream_stop"
CONF_ON_IMAGE = "on_image"
CONF_ON_CAPTURE_IMAGE = "on_capture_image"
CONF_ON_OVERLAY = "on_overlay"

camera_ns = cg.esphome_ns.namespace("camera")
Camera = camera_ns.class_("CameraImpl", cg.Component, cg.EntityBase)

CameraImageData = camera_ns.struct("CameraImageData")
CameraImageSpec = camera_ns.struct("CameraImageSpec")
CameraIncrementalContext = camera_ns.struct("CameraIncrementalContext")
CameraIncrementalContextRef = CameraIncrementalContext.operator("ref")

CameraImageImpl = camera_ns.class_("CameraImageImpl")
CameraImageTrigger = camera_ns.class_(
    "CameraImageTrigger", automation.Trigger.template()
)
CameraStreamStartTrigger = camera_ns.class_(
    "CameraStreamStartTrigger",
    automation.Trigger.template(),
)
CameraStreamStopTrigger = camera_ns.class_(
    "CameraStreamStopTrigger",
    automation.Trigger.template(),
)
CameraCaptureImageTrigger = camera_ns.class_(
    "CameraCaptureImageTrigger", automation.Trigger.template()
)
CameraOverlayTrigger = camera_ns.class_(
    "CameraOverlayTrigger", automation.Trigger.template()
)

IS_PLATFORM_COMPONENT = True
MULTI_CONF = True
MULTI_CONF_NO_DEFAULT = True

CAMERA_AUTOMATION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ON_STREAM_START): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CameraStreamStartTrigger),
            }
        ),
        cv.Optional(CONF_ON_STREAM_STOP): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CameraStreamStopTrigger),
            }
        ),
        cv.Optional(CONF_ON_IMAGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CameraImageTrigger),
            }
        ),
        cv.Optional(CONF_ON_CAPTURE_IMAGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    CameraCaptureImageTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_OVERLAY): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CameraOverlayTrigger),
            }
        ),
    }
)

_CAMERA_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Camera),
            cv.Required(CONF_CAMERA_ENCODER_ID): cv.use_id(camera_encoder.Encoder),
            cv.Required(CONF_CAMERA_SENSOR_ID): cv.use_id(camera_sensor.Sensor),
            cv.Optional(CONF_IDLE_UPDATE_INTERVAL, default=0): cv.int_range(0),
            cv.Optional(CONF_MAX_UPDATE_INTERVAL, default=100): cv.int_range(0),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(CAMERA_AUTOMATION_SCHEMA)
    .extend(cv.ENTITY_BASE_SCHEMA)
)


def camera_schema(
    class_: MockObjClass = cv.UNDEFINED,
) -> cv.Schema:
    schema = {}

    if class_ is not cv.UNDEFINED:
        # Not optional.
        schema[cv.GenerateID()] = cv.declare_id(class_)

    return _CAMERA_SCHEMA.extend(schema)


async def setup_camera(var, config):
    cg.add(var.set_idle_update_interval(config[CONF_IDLE_UPDATE_INTERVAL]))
    cg.add(var.set_max_update_interval(config[CONF_MAX_UPDATE_INTERVAL]))

    await setup_entity(var, config, "camera")
    await setup_camera_automation(var, config)
    await cg.register_component(var, config)
    if CONF_CAMERA_SENSOR_ID in config:
        sensor = await cg.get_variable(config[CONF_CAMERA_SENSOR_ID])
        cg.add(var.set_sensor(sensor))
    if CONF_CAMERA_ENCODER_ID in config:
        encoder = await cg.get_variable(config[CONF_CAMERA_ENCODER_ID])
        cg.add(var.set_encoder(encoder))


async def setup_camera_automation(var, config):
    for conf in config.get(CONF_ON_STREAM_START, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_STREAM_STOP, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_IMAGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(CameraImageData, "image")], conf)

    for conf in config.get(CONF_ON_CAPTURE_IMAGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [
                (CameraImageData, "image"),
                (CameraImageSpec, "spec"),
                (CameraIncrementalContextRef, "context"),
            ],
            conf,
        )

    for conf in config.get(CONF_ON_OVERLAY, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [
                (CameraImageData, "image"),
                (CameraImageSpec, "spec"),
                (CameraIncrementalContextRef, "context"),
            ],
            conf,
        )


async def new_camera(config, *args):
    cg.add_define("USE_CAMERA")
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await setup_camera(var, config)
    return var


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(camera_ns.using)
    cg.add_define("USE_CAMERA")
