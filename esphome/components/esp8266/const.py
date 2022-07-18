import esphome.codegen as cg

KEY_ESP8266 = "esp8266"
KEY_BOARD = "board"
KEY_PIN_INITIAL_STATES = "pin_initial_states"
CONF_RESTORE_FROM_FLASH = "restore_from_flash"
CONF_EARLY_PIN_INIT = "early_pin_init"

# esp8266 namespace is already defined by arduino, manually prefix esphome
esp8266_ns = cg.global_ns.namespace("esphome").namespace("esp8266")
