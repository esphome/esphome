from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.cpp_generator import MockObjClass

CONF_CD_PIN = "cd_pin"
IS_PLATFORM_COMPONENT = True
CODEOWNERS = ["@abel-msk"]

storage_ns = cg.esphome_ns.namespace("storage")
Storage = storage_ns.class_("Storage")

StorageIsPresent = storage_ns.class_(
    "StorageIsPresent", automation.Condition.template()
)

#
#   Validators
#


def validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


#
#   Schemas
#
def storage_schema(
    class_: MockObjClass,
) -> cv.Schema:
    return cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(class_),
        }
    )


#
#   To code
#


async def new_storage(config, *args):
    return cg.new_Pvariable(config[CONF_ID], *args)


#
#   Triggers/Conditions
#


@automation.register_condition(
    "storage.is_present",
    StorageIsPresent,
    automation.maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.use_id(Storage),
        }
    ),
)
async def storage_is_present_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)
