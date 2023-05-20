from esphome import automation
import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.automation import maybe_simple_id
from esphome.const import CONF_ID, CONF_TRIGGER_ID
from esphome.core import CORE
from esphome.coroutine import coroutine_with_priority


CODEOWNERS = ["@jesserockz"]

IS_PLATFORM_COMPONENT = True

CONF_ON_DATA = "on_data"

microphone_ns = cg.esphome_ns.namespace("microphone")

Microphone = microphone_ns.class_("Microphone")

CaptureAction = microphone_ns.class_(
    "CaptureAction", automation.Action, cg.Parented.template(Microphone)
)
StopCaptureAction = microphone_ns.class_(
    "StopCaptureAction", automation.Action, cg.Parented.template(Microphone)
)


DataTrigger = microphone_ns.class_(
    "DataTrigger",
    automation.Trigger.template(cg.std_vector.template(cg.int16).operator("ref")),
)

IsCapturingCondition = microphone_ns.class_(
    "IsCapturingCondition", automation.Condition
)


async def setup_microphone_core_(var, config):
    for conf in config.get(CONF_ON_DATA, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger,
            [(cg.std_vector.template(cg.uint8).operator("ref").operator("const"), "x")],
            conf,
        )


async def register_microphone(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    await setup_microphone_core_(var, config)


MICROPHONE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ON_DATA): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DataTrigger),
            }
        ),
    }
)


MICROPHONE_ACTION_SCHEMA = maybe_simple_id({cv.GenerateID(): cv.use_id(Microphone)})


async def media_player_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


automation.register_action(
    "microphone.capture", CaptureAction, MICROPHONE_ACTION_SCHEMA
)(media_player_action)

automation.register_action(
    "microphone.stop_capture", StopCaptureAction, MICROPHONE_ACTION_SCHEMA
)(media_player_action)

automation.register_condition(
    "microphone.is_capturing", IsCapturingCondition, MICROPHONE_ACTION_SCHEMA
)(media_player_action)


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(microphone_ns.using)
    cg.add_define("USE_MICROPHONE")
