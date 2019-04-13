import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_CSS_URL, CONF_ID, CONF_JS_URL, CONF_PORT
from esphome.core import CORE


WebServer = esphome_ns.class_('WebServer', Component, StoringController)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(WebServer),
    cv.Optional(CONF_PORT): cv.port,
    cv.Optional(CONF_CSS_URL): cv.string,
    cv.Optional(CONF_JS_URL): cv.string,
}).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(40.0)
def to_code(config):
    rhs = App.init_web_server(config.get(CONF_PORT))
    web_server = Pvariable(config[CONF_ID], rhs)
    if CONF_CSS_URL in config:
        cg.add(web_server.set_css_url(config[CONF_CSS_URL]))
    if CONF_JS_URL in config:
        cg.add(web_server.set_js_url(config[CONF_JS_URL]))

    register_component(web_server, config)


REQUIRED_BUILD_FLAGS = '-DUSE_WEB_SERVER'


def lib_deps(config):
    deps = []
    if CORE.is_esp32:
        deps.append('FS')
    deps.append('ESP Async WebServer@1.1.1')
    return deps
