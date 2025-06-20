import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["iwr6843aop"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(number.Number),
        cv.Optional("USER ID", default=1): cv.int_,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_int_output(config["USER ID"]))
