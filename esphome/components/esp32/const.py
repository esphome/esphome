import esphome.codegen as cg

KEY_ESP32 = "esp32"
KEY_BOARD = "board"

VARIANTS = ["ESP32", "ESP32S2", "ESP32S3", "ESP32C3", "ESP32H2"]

esp32_ns = cg.esphome_ns.namespace("esp32")
