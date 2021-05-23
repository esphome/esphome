# Dummy package to allow components to depend on network
import esphome.codegen as cg
from esphome.core import CORE

CODEOWNERS = ["@esphome/core"]


def add_mdns_library():
    cg.add_define("USE_MDNS")
    if CORE.is_esp32:
        cg.add_library("ESPmDNS", None)
    elif CORE.is_esp8266:
        cg.add_library("ESP8266mDNS", None)
