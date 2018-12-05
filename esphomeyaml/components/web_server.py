import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_CSS_URL, CONF_ID, CONF_JS_URL, CONF_PORT
from esphomeyaml.core import CORE
from esphomeyaml.cpp_generator import Pvariable, add
from esphomeyaml.cpp_helpers import setup_component
from esphomeyaml.cpp_types import esphomelib_ns, StoringController, Component, App

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
    deps = []
    if CORE.is_esp32:
        deps.append('FS')
    deps.append('ESP Async WebServer@1.1.1')
    return deps
