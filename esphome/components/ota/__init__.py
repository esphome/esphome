import logging

import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_PASSWORD, CONF_PORT, CONF_SAFE_MODE
from esphome.core import CORE
from esphome.cpp_generator import Pvariable, add, add_library
from esphome.cpp_helpers import register_component
from esphome.cpp_types import Component

_LOGGER = logging.getLogger(__name__)

ota_ns = cg.esphome_ns.namespace('ota')
OTAComponent = ota_ns.class_('OTAComponent', Component)


def default_port():
    if CORE.is_esp32:
        return 3232
    if CORE.is_esp8266:
        return 8266


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(OTAComponent),
    cv.Optional(CONF_SAFE_MODE, default=True): cv.boolean,
    cv.Optional(CONF_PORT, default=default_port): cv.port,
    cv.Optional(CONF_PASSWORD, default=''): cv.string,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    rhs = OTAComponent.new(config[CONF_PORT])
    ota = Pvariable(config[CONF_ID], rhs)
    add(ota.set_auth_password(config[CONF_PASSWORD]))
    if config[CONF_SAFE_MODE]:
        add(ota.start_safe_mode())

    if CORE.is_esp8266:
        add_library('Update', None)
    elif CORE.is_esp32:
        add_library('Hash', None)

    # Register at end for safe mode
    yield register_component(ota, config)
