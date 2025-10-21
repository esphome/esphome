from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CoroPriority, coroutine_with_priority

IS_PLATFORM_COMPONENT = True
CODEOWNERS = ["@abel-msk"]

storage_ns = cg.esphome_ns.namespace("storage")
Storage = storage_ns.class_("Storage")
StorageIsPresentCondition = storage_ns.class_(
    "StorageIsPresentCondition", automation.Condition.template()
)


@automation.register_condition(
    "storage.is_present",
    StorageIsPresentCondition,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(Storage),
        }
    ),
)
async def storage_is_present_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config):
    cg.add_define("USE_STORAGE_DEVICE")
    cg.add_global(storage_ns.using)
