import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@RobertJN64"]
DEPENDENCIES = ["time"]

ntp_server_ns = cg.esphome_ns.namespace("ntp_server")
NtpServer = ntp_server_ns.class_("NtpServer", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(NtpServer),
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
