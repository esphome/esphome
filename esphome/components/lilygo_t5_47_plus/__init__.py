"""Support for LILYGO T5 4.7" Plus E-Paper Display."""

import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.esp32.const import VARIANT_ESP32S3
import esphome.config_validation as cv

CODEOWNERS = ["@hbast"]
DEPENDENCIES = ["esp32", "psram"]
MULTI_CONF = False

CONFIG_SCHEMA = cv.All(
    cv.Schema({}),
    esp32.only_on_variant(supported=[VARIANT_ESP32S3]),
)

lilygo_t5_47_plus_ns = cg.esphome_ns.namespace("lilygo_t5_47_plus")
