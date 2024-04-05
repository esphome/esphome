from esphome import config_validation as cv
from esphome import codegen as cg
from esphome.const import CONF_ID, CONF_NAME

# ns = cg.esphome_ns.namespace("device")
# DeviceClass = ns.Class("Device")
StringRef = cg.esphome_ns.struct("StringRef")

MULTI_CONF = True

CODEOWNERS = ["@dala318"]

CONFIG_SCHEMA = cv.Schema(
    {
        # cv.Required(CONF_ID): cv.declare_id(DeviceClass),
        cv.Required(CONF_ID): cv.declare_id(StringRef),
        cv.Required(CONF_NAME): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_NAME],
    )
    # cg.add_define("USE_DEVICE_ID")
