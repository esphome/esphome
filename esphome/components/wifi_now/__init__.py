import esphome.codegen as cg
from esphome.core import CORE, coroutine

from . import component
from . import inject_action
from . import send_action
from . import terminal_action

CONFIG_SCHEMA = component.COMPONENT_SCHEMA


@coroutine
def to_code(config):
    cg.add_define('USE_WIFINOW')
    if CORE.is_esp8266:
        cg.add_library('ESP8266WiFi', None)
    yield component.wifi_now_component_to_code(config)
