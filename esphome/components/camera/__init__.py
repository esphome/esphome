from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_DELAY, CONF_PRIORITY, CONF_TRIGGER_ID
from esphome.core import coroutine_with_priority
from esphome.core.entity_helpers import setup_entity

CODEOWNERS = ["@DT-art1", "@bdraco"]

CONF_CAMERA_ID = "camera_id"
CONF_CORE = "core"
CONF_IDLE_UPDATE_INTERVAL = "idle_update_interval"
CONF_TASK_ID = "task_id"
CONF_MAX_UPDATE_INTERVAL = "max_update_interval"
CONF_PIPELINE_ID = "pipeline_id"
CONF_STACK_SIZE = "stack_size"
CONF_STATISTICS = "statistics"

CONF_ON_STREAM_START = "on_stream_start"
CONF_ON_STREAM_STOP = "on_stream_stop"
CONF_ON_IMAGE = "on_image"

camera_ns = cg.esphome_ns.namespace("camera")
Camera = camera_ns.class_("CameraImpl", cg.Component, cg.EntityBase)
Task = camera_ns.class_("Task")
Pipeline = camera_ns.class_("Pipeline")

CameraImageData = camera_ns.struct("CameraImageData")
CameraImageSpec = camera_ns.struct("CameraImageSpec")

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
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(CONF_CAMERA_ID): cv.declare_id(Camera),
            cv.Optional(CONF_IDLE_UPDATE_INTERVAL, default=0): cv.int_range(0),
            cv.Optional(CONF_MAX_UPDATE_INTERVAL, default=100): cv.int_range(0),
            cv.Optional(CONF_CORE, default=1): cv.int_range(0, 1),
            cv.Optional(CONF_STACK_SIZE, default=4096): cv.int_range(512, 32768),
            cv.Optional(CONF_PRIORITY, default=1): cv.int_range(0, 10),
            cv.Optional(CONF_DELAY, default=5): cv.int_range(1, 2000),
            cv.Optional(CONF_STATISTICS, default=False): cv.boolean,
            cv.GenerateID(CONF_PIPELINE_ID): cv.declare_id(Pipeline),
            cv.GenerateID(CONF_TASK_ID): cv.declare_id(Task),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(CAMERA_AUTOMATION_SCHEMA)
    .extend(cv.ENTITY_BASE_SCHEMA)
)


async def setup_camera(var, config):
    cg.add(var.set_idle_update_interval(config[CONF_IDLE_UPDATE_INTERVAL]))
    cg.add(var.set_max_update_interval(config[CONF_MAX_UPDATE_INTERVAL]))
    cg.add(var.set_statistics(config[CONF_STATISTICS]))
    task = cg.new_Pvariable(
        config[CONF_TASK_ID],
        config[CONF_CORE],
        config[CONF_STACK_SIZE],
        config[CONF_PRIORITY],
        config[CONF_DELAY],
    )
    cg.add(var.set_task(task))
    pipeline = cg.new_Pvariable(config[CONF_PIPELINE_ID])
    cg.add(var.set_pipeline(pipeline))
    await setup_entity(var, config, "camera")
    await setup_camera_automation(var, config)
    await cg.register_component(var, config)


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


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(camera_ns.using)
    cg.add_define("USE_CAMERA")
    var = cg.new_Pvariable(config[CONF_CAMERA_ID])
    await setup_camera(var, config)
