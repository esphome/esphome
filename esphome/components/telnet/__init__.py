import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@apbodrov"]

telnet_ns = cg.esphome_ns.namespace("telnet")
TelnetComponent = telnet_ns.class_("TelnetComponent", cg.Component)


CONFIG_SCHEMA = cv.All(
    cv.Schema({cv.GenerateID(): cv.declare_id(TelnetComponent)}),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add_library("lennarthennigs/ESP Telnet", "2.2.1")
    await cg.register_component(var, config)
