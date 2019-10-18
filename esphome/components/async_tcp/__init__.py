# Dummy integration to allow relying on AsyncTCP
from esphome.core import CORE, coroutine_with_priority
import esphome.codegen as cg


@coroutine_with_priority(200.0)
def to_code(config):
    if CORE.is_esp32:
        # https://github.com/me-no-dev/AsyncTCP/blob/master/library.json
        cg.add_library('AsyncTCP', '1.0.3')
    elif CORE.is_esp8266:
        # https://github.com/OttoWinter/ESPAsyncTCP
        cg.add_library('ESPAsyncTCP-esphome', '1.2.2')
