import voluptuous as vol

from esphomeyaml import core
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_CSS_URL, CONF_ID, CONF_JS_URL, CONF_PORT, ESP_PLATFORM_ESP32
from esphomeyaml.helpers import App, Component, Pvariable, StoringController, add, esphomelib_ns, \
    setup_component

WebServer = esphomelib_ns.class_('WebServer', Component, StoringController)

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(WebServer),
    vol.Optional(CONF_PORT): cv.port,
    vol.Optional(CONF_CSS_URL): cv.string,
    vol.Optional(CONF_JS_URL): cv.string,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.init_web_server(config.get(CONF_PORT))
    web_server = Pvariable(config[CONF_ID], rhs)
    if CONF_CSS_URL in config:
        add(web_server.set_css_url(config[CONF_CSS_URL]))
    if CONF_JS_URL in config:
        add(web_server.set_js_url(config[CONF_JS_URL]))

    setup_component(web_server, config)


BUILD_FLAGS = '-DUSE_WEB_SERVER'


def lib_deps(config):
    if core.ESP_PLATFORM == ESP_PLATFORM_ESP32:
        return 'FS'
    return ''
