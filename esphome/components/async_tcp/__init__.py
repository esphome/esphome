# Dummy integration to allow relying on AsyncTCP
import esphome.codegen as cg
from esphome.const import ARDUINO_VERSION_ESP32_1_0_0, ARDUINO_VERSION_ESP32_1_0_1, \
    ARDUINO_VERSION_ESP32_1_0_2
from esphome.core import CORE, coroutine_with_priority


@coroutine_with_priority(200.0)
def to_code(config):
    if CORE.is_esp32:
        # https://github.com/me-no-dev/AsyncTCP/blob/master/library.json
        versions_requiring_older_asynctcp = [
            ARDUINO_VERSION_ESP32_1_0_0,
            ARDUINO_VERSION_ESP32_1_0_1,
            ARDUINO_VERSION_ESP32_1_0_2,
        ]
        if CORE.arduino_version in versions_requiring_older_asynctcp:
            cg.add_library('AsyncTCP', '1.0.3')
        else:
            cg.add_library('AsyncTCP', '1.1.1')
    elif CORE.is_esp8266:
        # https://github.com/OttoWinter/ESPAsyncTCP
        cg.add_library('ESPAsyncTCP-esphome', '1.2.2')
