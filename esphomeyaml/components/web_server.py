import logging

import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_PORT
from esphomeyaml.helpers import App, add

_LOGGER = logging.getLogger(__name__)

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID('web_server'): cv.register_variable_id,
    vol.Optional(CONF_PORT): cv.port,
})


def to_code(config):
    add(App.init_web_server(config.get(CONF_PORT)))


def build_flags(config):
    return '-DUSE_WEB_SERVER'
