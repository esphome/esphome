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
    CONF_INCLUDE_INTERNAL,
    CONF_OTA,
    CONF_VERSION,
    CONF_LOCAL,
)
from esphome.core import CORE, coroutine_with_priority

AUTO_LOAD = ["json", "web_server_base"]


web_server_ns = cg.esphome_ns.namespace("web_server")
WebServer = web_server_ns.class_("WebServer", cg.Component, cg.Controller)


def default_url(config):
    config = config.copy()
    if config[CONF_VERSION] == 1:
        if not (CONF_CSS_URL in config):
            config[CONF_CSS_URL] = "https://esphome.io/_static/webserver-v1.min.css"
        if not (CONF_JS_URL in config):
            config[CONF_JS_URL] = "https://esphome.io/_static/webserver-v1.min.js"
    if config[CONF_VERSION] == 2:
        if not (CONF_CSS_URL in config):
            config[CONF_CSS_URL] = ""
        if not (CONF_JS_URL in config):
            config[CONF_JS_URL] = "https://oi.esphome.io/v2/www.js"
    return config


def validate_local(config):
    if CONF_LOCAL in config and config[CONF_VERSION] == 1:
        raise cv.Invalid("'local' is not supported in version 1")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WebServer),
            cv.Optional(CONF_PORT, default=80): cv.port,
            cv.Optional(CONF_VERSION, default=2): cv.one_of(1, 2),
            cv.Optional(CONF_CSS_URL): cv.string,
            cv.Optional(CONF_CSS_INCLUDE): cv.file_,
            cv.Optional(CONF_JS_URL): cv.string,
            cv.Optional(CONF_JS_INCLUDE): cv.file_,
            cv.Optional(CONF_AUTH): cv.Schema(
                {
                    cv.Required(CONF_USERNAME): cv.All(
                        cv.string_strict, cv.Length(min=1)
                    ),
                    cv.Required(CONF_PASSWORD): cv.All(
                        cv.string_strict, cv.Length(min=1)
                    ),
                }
            ),
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
            cv.Optional(CONF_INCLUDE_INTERNAL, default=False): cv.boolean,
            cv.Optional(CONF_OTA, default=True): cv.boolean,
            cv.Optional(CONF_LOCAL): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_arduino,
    cv.only_on(["esp32", "esp8266"]),
    default_url,
    validate_local,
)


def build_index_html(config):
    html = "<!DOCTYPE html><html><head><meta charset=UTF-8><link rel=icon href=data:>"
    if config[CONF_CSS_INCLUDE]:
        html += '<link rel="stylesheet" href="/0.css">'
    if config[CONF_CSS_URL]:
        html += '<link rel="stylesheet" href="{config[CONF_CSS_URL]}">'
    html += "</head><body>"
    if config[CONF_JS_INCLUDE]:
        html += '<script type="module" src="/0.js"></script>"'
    html += "<esp-app></esp-app>"
    if config[CONF_JS_URL]:
        html += f'<script type="module" src="{config[CONF_JS_URL]}"></script>"'
    html += "</body></html>"
    return html


@coroutine_with_priority(40.0)
async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)

    cg.add_define("USE_WEBSERVER")
    version = config[CONF_VERSION]

    cg.add(paren.set_port(config[CONF_PORT]))
    cg.add_define("USE_WEBSERVER")
    cg.add_define("USE_WEBSERVER_PORT", config[CONF_PORT])
    cg.add_define("USE_WEBSERVER_VERSION", version)
    if version == 2:
        cg.add(var.set_index_html(build_index_html(config)))
    else:
        cg.add(var.set_css_url(config[CONF_CSS_URL]))
        cg.add(var.set_js_url(config[CONF_JS_URL]))
    cg.add(var.set_allow_ota(config[CONF_OTA]))
    if CONF_AUTH in config:
        cg.add(paren.set_auth_username(config[CONF_AUTH][CONF_USERNAME]))
        cg.add(paren.set_auth_password(config[CONF_AUTH][CONF_PASSWORD]))
    if CONF_CSS_INCLUDE in config:
        cg.add_define("USE_WEBSERVER_CSS_INCLUDE")
        path = CORE.relative_config_path(config[CONF_CSS_INCLUDE])
        with open(file=path, encoding="utf-8") as myfile:
            cg.add(var.set_css_include(myfile.read()))
    if CONF_JS_INCLUDE in config:
        cg.add_define("USE_WEBSERVER_JS_INCLUDE")
        path = CORE.relative_config_path(config[CONF_JS_INCLUDE])
        with open(file=path, encoding="utf-8") as myfile:
            cg.add(var.set_js_include(myfile.read()))
    cg.add(var.set_include_internal(config[CONF_INCLUDE_INTERNAL]))
    if CONF_LOCAL in config and config[CONF_LOCAL]:
        cg.add_define("USE_WEBSERVER_LOCAL")
