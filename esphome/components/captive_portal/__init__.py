import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.const import CONF_ID
from esphome.core import coroutine_with_priority

AUTO_LOAD = ["web_server_base"]
DEPENDENCIES = ["wifi"]
CODEOWNERS = ["@OttoWinter"]

captive_portal_ns = cg.esphome_ns.namespace("captive_portal")
CaptivePortal = captive_portal_ns.class_("CaptivePortal", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CaptivePortal),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
            web_server_base.WebServerBase
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(64.0)
def to_code(config):
    paren = yield cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    var = cg.new_Pvariable(config[CONF_ID], paren)
    yield cg.register_component(var, config)
    cg.add_define("USE_CAPTIVE_PORTAL")
