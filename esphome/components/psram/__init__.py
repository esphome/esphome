import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.core import CORE

CODEOWNERS = ["@esphome/core"]

CONFIG_SCHEMA = cv.All(cv.Schema({}), cv.only_on_esp32)


async def to_code(config):
    if CORE.using_arduino:
        cg.add_build_flag("-DBOARD_HAS_PSRAM")

    if CORE.using_esp_idf:
        add_idf_sdkconfig_option("CONFIG_ESP32_SPIRAM_SUPPORT", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM_USE_CAPS_ALLOC", True)
        add_idf_sdkconfig_option("CONFIG_SPIRAM_IGNORE_NOTFOUND", True)
