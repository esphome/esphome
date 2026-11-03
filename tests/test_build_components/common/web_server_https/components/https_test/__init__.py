"""Test-only component that serves HTTPS through the web_server_idf developer API."""

import esphome.codegen as cg
from esphome.components import web_server_idf
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID, WebServerBase
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

AUTO_LOAD = ["web_server_base"]

https_test_ns = cg.esphome_ns.namespace("https_test")
HttpsTest = https_test_ns.class_("HttpsTest", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(HttpsTest),
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(WebServerBase),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    web_server_idf.consume_ssl_sockets,
)


async def to_code(config: ConfigType) -> None:
    web_server_idf.enable_ssl()
    base = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])
    var = cg.new_Pvariable(config[CONF_ID], base)
    await cg.register_component(var, config)
