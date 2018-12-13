import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_PORT, CONF_PASSWORD
from esphomeyaml.core import CORE
from esphomeyaml.cpp_generator import Pvariable, add
from esphomeyaml.cpp_helpers import setup_component
from esphomeyaml.cpp_types import App, Component, StoringController, esphomelib_ns

api_ns = esphomelib_ns.namespace('api')
APIServer = api_ns.class_('APIServer', Component, StoringController)

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(APIServer),
    vol.Optional(CONF_PORT, default=6053): cv.port,
    vol.Optional(CONF_PASSWORD, default=''): cv.string_strict,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.init_api_server()
    api = Pvariable(config[CONF_ID], rhs)

    if CONF_PORT in config:
        add(api.set_port(config[CONF_PORT]))
    if CONF_PASSWORD in config:
        add(api.set_password(config[CONF_PASSWORD]))

    setup_component(api, config)


BUILD_FLAGS = '-DUSE_API'


def lib_deps(config):
    if CORE.is_esp32:
        return 'AsyncTCP@1.0.1'
    elif CORE.is_esp8266:
        return 'ESPAsyncTCP@1.1.3'
    raise NotImplementedError
