import esphome.codegen as cg
from esphome.core import CORE, coroutine

from . import component

# the following imports referenced implicitly by @automation.register_action
from . import inject_action  # noqa: F401
from . import send_action  # noqa: F401
from . import terminal_action  # noqa: F401

CONFIG_SCHEMA = component.COMPONENT_SCHEMA

CODEOWNERS = ["@motwok"]

@coroutine
def to_code(config):
    cg.add_define('WIFI_BASIC_COOP')
    cg.add_define('USE_WIFINOW')
    if CORE.is_esp8266:
        cg.add_library('ESP8266WiFi', None)
    yield component.wifi_now_component_to_code(config)
