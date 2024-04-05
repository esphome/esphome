from esphome import config_validation as cv
from esphome import codegen as cg
from esphome.const import CONF_ID, CONF_NAME

DeviceStruct = cg.esphome_ns.struct("Device")

MULTI_CONF = True


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(DeviceStruct),
        cv.Required(CONF_NAME): cv.string,
        # cv.Exclusive(CONF_RED, "red"): cv.percentage,
        # cv.Exclusive(CONF_RED_INT, "red"): cv.uint8_t,
        # cv.Exclusive(CONF_GREEN, "green"): cv.percentage,
        # cv.Exclusive(CONF_GREEN_INT, "green"): cv.uint8_t,
        # cv.Exclusive(CONF_BLUE, "blue"): cv.percentage,
        # cv.Exclusive(CONF_BLUE_INT, "blue"): cv.uint8_t,
        # cv.Exclusive(CONF_WHITE, "white"): cv.percentage,
        # cv.Exclusive(CONF_WHITE_INT, "white"): cv.uint8_t,
    }).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    # paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    # var = cg.new_Pvariable(config[CONF_ID], paren)
    # await cg.register_component(var, config)
    # cg.add_define("USE_CAPTIVE_PORTAL")

    cg.new_variable(
        config[CONF_ID],
        cg.new_Pvariable(config[CONF_NAME]),
    )
    # cg.add_define("USE_DEVICE_ID")
