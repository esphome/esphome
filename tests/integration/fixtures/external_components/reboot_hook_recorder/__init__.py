import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/tests"]
MULTI_CONF = True

reboot_hook_recorder_ns = cg.esphome_ns.namespace("reboot_hook_recorder")
RebootHookRecorder = reboot_hook_recorder_ns.class_("RebootHookRecorder", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RebootHookRecorder),
        cv.Required(CONF_NAME): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_name(config[CONF_NAME]))
