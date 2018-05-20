import hashlib
import logging

import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import core
from esphomeyaml.const import CONF_ID, CONF_OTA, CONF_PASSWORD, CONF_PORT, CONF_SAFE_MODE, \
    ESP_PLATFORM_ESP32, ESP_PLATFORM_ESP8266
from esphomeyaml.core import ESPHomeYAMLError
from esphomeyaml.helpers import App, Pvariable, add, esphomelib_ns

_LOGGER = logging.getLogger(__name__)

CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(CONF_OTA): cv.register_variable_id,
    vol.Optional(CONF_SAFE_MODE, default=True): cv.boolean,
    # TODO Num attempts + wait time
    vol.Optional(CONF_PORT): cv.port,
    vol.Optional(CONF_PASSWORD): cv.string,
})

OTAComponent = esphomelib_ns.OTAComponent


def to_code(config):
    rhs = App.init_ota()
    ota = Pvariable(OTAComponent, config[CONF_ID], rhs)
    if CONF_PASSWORD in config:
        hash_ = hashlib.md5(config[CONF_PASSWORD].encode()).hexdigest()
        add(ota.set_auth_password_hash(hash_))
    if config[CONF_SAFE_MODE]:
        add(ota.start_safe_mode())


def get_port(config):
    if CONF_PORT in config[CONF_OTA]:
        return config[CONF_OTA][CONF_PORT]
    if core.ESP_PLATFORM == ESP_PLATFORM_ESP32:
        return 3232
    elif core.ESP_PLATFORM == ESP_PLATFORM_ESP8266:
        return 8266
    raise ESPHomeYAMLError(u"Invalid ESP Platform for ESP OTA port.")


def get_auth(config):
    return config[CONF_OTA].get(CONF_PASSWORD, '')


BUILD_FLAGS = '-DUSE_OTA'


def lib_deps(config):
    if core.ESP_PLATFORM == ESP_PLATFORM_ESP32:
        return ['ArduinoOTA', 'Update', 'ESPmDNS']
    return ['Hash', 'ESP8266mDNS', 'ArduinoOTA']
