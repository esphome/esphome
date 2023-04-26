import gzip
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
#this import statement should make testing the PR much easier since othwerwise you get errors from const.py since it doesn't import with a custom component
try:
    from esphome.const import (
        CONF_CSS_INCLUDE_ARRAY,
        CONF_JS_INCLUDE_ARRAY,
        CONF_LOCAL_PAGE_INCLUDE,
        CONF_LOCAL_PAGE_INCLUDE_ARRAY,
    )
    globals()['CONF_CSS_INCLUDE_ARRAY'] = CONF_CSS_INCLUDE_ARRAY
    globals()['CONF_JS_INCLUDE_ARRAY'] = CONF_JS_INCLUDE_ARRAY
    globals()['CONF_LOCAL_PAGE_INCLUDE'] = CONF_LOCAL_PAGE_INCLUDE 
    globals()['CONF_LOCAL_PAGE_INCLUDE_ARRAY'] = CONF_LOCAL_PAGE_INCLUDE_ARRAY
except ImportError:
    globals()['CONF_JS_INCLUDE_ARRAY'] = "js_include_array"
    globals()['CONF_CSS_INCLUDE_ARRAY'] = "css_include_array"
    globals()['CONF_LOCAL_PAGE_INCLUDE'] = "local_page_include"
    globals()['CONF_LOCAL_PAGE_INCLUDE_ARRAY'] = "local_page_include_array"

from esphome.core import CORE, coroutine_with_priority, HexInt

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


def load_file_bytes_to_gzip_hex_int_array(path):
    with open(file=path, encoding="utf-8") as myfile:
        loaded_file = myfile.read()
        file_gzipped = gzip.compress(bytes(loaded_file, "utf-8"))
        rhs = [HexInt(x) for x in file_gzipped]
        print(
            "Compressed file:"
            + path
            + "\n\tOld size: "
            + str(len(loaded_file))
            + "\n\tNew size: "
            + str(len(file_gzipped))
            + " ("
            + str(int(len(file_gzipped) / len(loaded_file) * 100))
            + "% of orginal)"
        )
        return rhs


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WebServer),
            cv.Optional(CONF_PORT, default=80): cv.port,
            cv.Optional(CONF_VERSION, default=2): cv.one_of(1, 2),
            cv.Optional(CONF_CSS_URL): cv.string,
            cv.Optional(CONF_CSS_INCLUDE): cv.file_,
            cv.GenerateID(CONF_CSS_INCLUDE_ARRAY): cv.declare_id(cg.uint8),
            cv.Optional(CONF_JS_URL): cv.string,
            cv.Optional(CONF_JS_INCLUDE): cv.file_,
            cv.GenerateID(CONF_JS_INCLUDE_ARRAY): cv.declare_id(cg.uint8),
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
            cv.Optional(CONF_LOCAL_PAGE_INCLUDE): cv.file_,
            cv.GenerateID(CONF_LOCAL_PAGE_INCLUDE_ARRAY): cv.declare_id(cg.uint8),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_arduino,
    cv.only_on(["esp32", "esp8266"]),
    default_url,
    validate_local,
)


@coroutine_with_priority(40.0)
async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)

    cg.add_define("USE_WEBSERVER")

    cg.add(paren.set_port(config[CONF_PORT]))
    cg.add_define("USE_WEBSERVER")
    cg.add_define("USE_WEBSERVER_PORT", config[CONF_PORT])
    cg.add_define("USE_WEBSERVER_VERSION", config[CONF_VERSION])
    cg.add(var.set_css_url(config[CONF_CSS_URL]))
    cg.add(var.set_js_url(config[CONF_JS_URL]))
    cg.add(var.set_allow_ota(config[CONF_OTA]))
    if CONF_AUTH in config:
        cg.add(paren.set_auth_username(config[CONF_AUTH][CONF_USERNAME]))
        cg.add(paren.set_auth_password(config[CONF_AUTH][CONF_PASSWORD]))
    if CONF_CSS_INCLUDE in config:
        cg.add_define("USE_WEBSERVER_CSS_INCLUDE")
        path = CORE.relative_config_path(config[CONF_CSS_INCLUDE])
        rhs = load_file_bytes_to_gzip_hex_int_array(path)
        cg.add_define("CSS_INCLUDE_ARRAY_SIZE", len(rhs))
        prog_arr = cg.progmem_array(config[CONF_CSS_INCLUDE_ARRAY], rhs)
        cg.add(var.set_css_include(prog_arr))
    if CONF_JS_INCLUDE in config:
        cg.add_define("USE_WEBSERVER_JS_INCLUDE")
        path = CORE.relative_config_path(config[CONF_JS_INCLUDE])
        rhs = load_file_bytes_to_gzip_hex_int_array(path)
        cg.add_define("JS_INCLUDE_ARRAY_SIZE", len(rhs))
        prog_arr = cg.progmem_array(config[CONF_JS_INCLUDE_ARRAY], rhs)
        cg.add(var.set_js_include(prog_arr))
    cg.add(var.set_include_internal(config[CONF_INCLUDE_INTERNAL]))
    if CONF_LOCAL in config and config[CONF_LOCAL]:
        cg.add_define("USE_WEBSERVER_LOCAL")
    if CONF_LOCAL_PAGE_INCLUDE in config:
        cg.add_define("USE_WEBSERVER_LOCAL_PAGE_INCLUDE")
        path = CORE.relative_config_path(config[CONF_LOCAL_PAGE_INCLUDE])
        rhs = load_file_bytes_to_gzip_hex_int_array(path)
        cg.add_define("LOCAL_PAGE_ARRAY_SIZE", len(rhs))
        prog_arr = cg.progmem_array(config[CONF_LOCAL_PAGE_INCLUDE_ARRAY], rhs)
        cg.add(var.set_local_page_include(prog_arr))
