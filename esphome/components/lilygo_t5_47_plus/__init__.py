"""Support for LILYGO T5 4.7" Plus E-Paper Display."""

import esphome.codegen as cg

CODEOWNERS = ["@hbast"]
DEPENDENCIES = ["esp32", "psram"]
MULTI_CONF = False

lilygo_t5_47_plus_ns = cg.esphome_ns.namespace("lilygo_t5_47_plus")
