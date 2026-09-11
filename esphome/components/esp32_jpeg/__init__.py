import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.esp32 import VARIANT_ESP32P4, only_on_variant
import esphome.config_validation as cv

DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@kyvaith"]

CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
    cv.only_on_esp32,
    cv.only_with_framework("esp-idf"),
    only_on_variant(supported=[VARIANT_ESP32P4]),
)


async def to_code(config):
    cg.add_define("USE_ESP32_JPEG")
    esp32.include_builtin_idf_component("esp_driver_jpeg")
