# Dummy integration to allow relying on AsyncTCP
import esphome.codegen as cg
from esphome.core import CORE, coroutine_with_priority

CODEOWNERS = ["@OttoWinter"]


@coroutine_with_priority(200.0)
async def to_code(config):
    if CORE.is_esp32:
        # https://github.com/esphome/AsyncTCP/blob/master/library.json
        cg.add_library("esphome/AsyncTCP-esphome", "1.2.2")
    elif CORE.is_esp8266:
        # https://github.com/OttoWinter/ESPAsyncTCP
        cg.add_library("ESPAsyncTCP-esphome", "1.2.3")
