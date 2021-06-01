import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.const import (
    CONF_CSS_INCLUDE,
    CONF_CSS_URL,
    CONF_ID,
    CONF_JS_INCLUDE,
    CONF_JS_URL,
    CONF_PORT,
    CONF_AUTH,
    CONF_USERNAME,
    CONF_PASSWORD,
)
from esphome.core import coroutine_with_priority

AUTO_LOAD = ["json", "web_server_base"]

web_server_ns = cg.esphome_ns.namespace("web_server")
WebServer = web_server_ns.class_("WebServer", cg.Component, cg.Controller)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WebServer),
        cv.Optional(CONF_PORT, default=80): cv.port,
        cv.Optional(
            CONF_CSS_URL, default="https://esphome.io/_static/webserver-v1.min.css"
        ): cv.string,
        cv.Optional(CONF_CSS_INCLUDE): cv.file_,
        cv.Optional(
            CONF_JS_URL, default="https://esphome.io/_static/webserver-v1.min.js"
        ): cv.string,
        cv.Optional(CONF_JS_INCLUDE): cv.file_,
        cv.Optional(CONF_AUTH): cv.Schema(
            {
                cv.Required(CONF_USERNAME): cv.string_strict,
                cv.Required(CONF_PASSWORD): cv.string_strict,
            }
        ),
        cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
            web_server_base.WebServerBase
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(40.0)
async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)

    cg.add(paren.set_port(config[CONF_PORT]))
    cg.add_define("WEBSERVER_PORT", config[CONF_PORT])
    cg.add(var.set_css_url(config[CONF_CSS_URL]))
    cg.add(var.set_js_url(config[CONF_JS_URL]))
    if CONF_AUTH in config:
        cg.add(var.set_username(config[CONF_AUTH][CONF_USERNAME]))
        cg.add(var.set_password(config[CONF_AUTH][CONF_PASSWORD]))
    if CONF_CSS_INCLUDE in config:
        cg.add_define("WEBSERVER_CSS_INCLUDE")
        with open(config[CONF_CSS_INCLUDE], "r") as myfile:
            cg.add(var.set_css_include(myfile.read()))
    if CONF_JS_INCLUDE in config:
        cg.add_define("WEBSERVER_JS_INCLUDE")
        with open(config[CONF_JS_INCLUDE], "r") as myfile:
            cg.add(var.set_js_include(myfile.read()))
