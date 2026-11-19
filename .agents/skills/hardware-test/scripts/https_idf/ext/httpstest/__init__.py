import esphome.codegen as cg
from esphome.components import web_server_base, web_server_idf
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

DOMAIN = "httpstest"
DEPENDENCIES = ["network"]
AUTO_LOAD = ["web_server_base", "web_server_idf"]

httpstest_ns = cg.esphome_ns.namespace("httpstest")
HttpsTest = httpstest_ns.class_("HttpsTest", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HttpsTest),
            cv.GenerateID(web_server_base.CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    web_server_idf.consume_ssl_sockets,
)


async def to_code(config: ConfigType) -> None:
    base = await cg.get_variable(config[web_server_base.CONF_WEB_SERVER_BASE_ID])
    var = cg.new_Pvariable(config[CONF_ID], base)
    await cg.register_component(var, config)
