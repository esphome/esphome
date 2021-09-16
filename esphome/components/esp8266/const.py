import esphome.codegen as cg

KEY_ESP8266 = "esp8266"
KEY_BOARD = "board"

# esp8266 namespace is already defined by arduino, manually prefix esphome
esp8266_ns = cg.global_ns.namespace("esphome").namespace("esp8266")
