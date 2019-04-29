import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_CSS_URL, CONF_ID, CONF_JS_URL, CONF_PORT
from esphome.core import CORE, coroutine_with_priority

DEPENDENCIES = ['network']
AUTO_LOAD = ['json']

web_server_ns = cg.esphome_ns.namespace('web_server')
WebServer = web_server_ns.class_('WebServer', cg.Component, cg.Controller)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WebServer),
    cv.Optional(CONF_PORT, default=80): cv.port,
    cv.Optional(CONF_CSS_URL, default="https://esphome.io/_static/webserver-v1.min.css"): cv.string,
    cv.Optional(CONF_JS_URL, default="https://esphome.io/_static/webserver-v1.min.js"): cv.string,
}).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(40.0)
def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_css_url(config[CONF_CSS_URL]))
    cg.add(var.set_js_url(config[CONF_JS_URL]))

    if CORE.is_esp32:
        cg.add_library('FS', None)
    cg.add_library('ESP Async WebServer', '1.1.1')
