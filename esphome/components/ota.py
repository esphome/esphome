import logging

import voluptuous as vol

import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_OTA, CONF_PASSWORD, CONF_PORT, CONF_SAFE_MODE
from esphome.core import CORE
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_types import App, Component, esphome_ns

_LOGGER = logging.getLogger(__name__)

OTAComponent = esphome_ns.class_('OTAComponent', Component)

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(OTAComponent),
    vol.Optional(CONF_SAFE_MODE, default=True): cv.boolean,
    vol.Optional(CONF_PORT): cv.port,
    vol.Optional(CONF_PASSWORD): cv.string,
})


def to_code(config):
    rhs = App.init_ota()
    ota = Pvariable(config[CONF_ID], rhs)
    if CONF_PASSWORD in config:
        add(ota.set_auth_password(config[CONF_PASSWORD]))
    if CONF_PORT in config:
        add(ota.set_port(config[CONF_PORT]))
    if config[CONF_SAFE_MODE]:
        add(ota.start_safe_mode())


def get_port(config):
    if CONF_PORT in config[CONF_OTA]:
        return config[CONF_OTA][CONF_PORT]
    if CORE.is_esp32:
        return 3232
    if CORE.is_esp8266:
        return 8266
    raise NotImplementedError


def get_auth(config):
    return config[CONF_OTA].get(CONF_PASSWORD, '')


BUILD_FLAGS = '-DUSE_OTA'
REQUIRED_BUILD_FLAGS = '-DUSE_NEW_OTA'


def lib_deps(config):
    if CORE.is_esp32:
        return ['Update', 'ESPmDNS']
    if CORE.is_esp8266:
        return ['Hash', 'ESP8266mDNS']
    raise NotImplementedError
