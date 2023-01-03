FLASH_SIZE_1_MB = 2**20
FLASH_SIZE_512_KB = FLASH_SIZE_1_MB // 2
FLASH_SIZE_2_MB = 2 * FLASH_SIZE_1_MB
FLASH_SIZE_4_MB = 4 * FLASH_SIZE_1_MB
FLASH_SIZE_16_MB = 16 * FLASH_SIZE_1_MB

ESP8266_LD_SCRIPTS = {
    FLASH_SIZE_512_KB: ("eagle.flash.512k0.ld", "eagle.flash.512k.ld"),
    FLASH_SIZE_1_MB: ("eagle.flash.1m0.ld", "eagle.flash.1m.ld"),
    FLASH_SIZE_2_MB: ("eagle.flash.2m.ld", "eagle.flash.2m.ld"),
    FLASH_SIZE_4_MB: ("eagle.flash.4m.ld", "eagle.flash.4m.ld"),
    FLASH_SIZE_16_MB: ("eagle.flash.16m.ld", "eagle.flash.16m14m.ld"),
}

ESP8266_BASE_PINS = {
    "A0": 17,
    "SS": 15,
    "MOSI": 13,
    "MISO": 12,
    "SCK": 14,
    "SDA": 4,
    "SCL": 5,
    "RX": 3,
    "TX": 1,
}

ESP8266_BOARD_PINS = {
    "d1": {
        "D0": 3,
        "D1": 1,
        "D2": 16,
        "D3": 5,
        "D4": 4,
        "D5": 14,
        "D6": 12,
        "D7": 13,
        "D8": 0,
        "D9": 2,
        "D10": 15,
        "D11": 13,
        "D12": 14,
        "D13": 14,
        "D14": 4,
        "D15": 5,
        "LED": 2,
    },
    "d1_mini": {
        "D0": 16,
        "D1": 5,
        "D2": 4,
        "D3": 0,
        "D4": 2,
        "D5": 14,
        "D6": 12,
        "D7": 13,
        "D8": 15,
        "LED": 2,
    },
    "d1_mini_lite": "d1_mini",
    "d1_mini_pro": "d1_mini",
    "esp01": {},
    "esp01_1m": {},
    "esp07": {},
    "esp12e": {},
    "esp210": {},
    "esp8285": {},
    "esp_wroom_02": {},
    "espduino": {"LED": 16},
    "espectro": {"LED": 15, "BUTTON": 2},
    "espino": {"LED": 2, "LED_RED": 2, "LED_GREEN": 4, "LED_BLUE": 5, "BUTTON": 0},
    "espinotee": {"LED": 16},
    "espmxdevkit": {},
    "espresso_lite_v1": {"LED": 16},
    "espresso_lite_v2": {"LED": 2},
    "gen4iod": {},
    "heltec_wifi_kit_8": "d1_mini",
    "huzzah": {
        "LED": 0,
        "LED_RED": 0,
        "LED_BLUE": 2,
        "D4": 4,
        "D5": 5,
        "D12": 12,
        "D13": 13,
        "D14": 14,
        "D15": 15,
        "D16": 16,
    },
    "inventone": {},
    "modwifi": {},
    "nodemcu": {
        "D0": 16,
        "D1": 5,
        "D2": 4,
        "D3": 0,
        "D4": 2,
        "D5": 14,
        "D6": 12,
        "D7": 13,
        "D8": 15,
        "D9": 3,
        "D10": 1,
        "LED": 16,
    },
    "nodemcuv2": "nodemcu",
    "oak": {
        "P0": 2,
        "P1": 5,
        "P2": 0,
        "P3": 3,
        "P4": 1,
        "P5": 4,
        "P6": 15,
        "P7": 13,
        "P8": 12,
        "P9": 14,
        "P10": 16,
        "P11": 17,
        "LED": 5,
    },
    "phoenix_v1": {"LED": 16},
    "phoenix_v2": {"LED": 2},
    "sonoff_basic": {},
    "sonoff_s20": {},
    "sonoff_sv": {},
    "sonoff_th": {},
    "sparkfunBlynk": "thing",
    "thing": {"LED": 5, "SDA": 2, "SCL": 14},
    "thingdev": "thing",
    "wifi_slot": {"LED": 2},
    "wifiduino": {
        "D0": 3,
        "D1": 1,
        "D2": 2,
        "D3": 0,
        "D4": 4,
        "D5": 5,
        "D6": 16,
        "D7": 14,
        "D8": 12,
        "D9": 13,
        "D10": 15,
        "D11": 13,
        "D12": 12,
        "D13": 14,
    },
    "wifinfo": {
        "LED": 12,
        "D0": 16,
        "D1": 5,
        "D2": 4,
        "D3": 0,
        "D4": 2,
        "D5": 14,
        "D6": 12,
        "D7": 13,
        "D8": 15,
        "D9": 3,
        "D10": 1,
    },
    "wio_link": {"LED": 2, "GROVE": 15, "D0": 14, "D1": 12, "D2": 13, "BUTTON": 0},
    "wio_node": {"LED": 2, "GROVE": 15, "D0": 3, "D1": 5, "BUTTON": 0},
    "xinabox_cw01": {"SDA": 2, "SCL": 14, "LED": 5, "LED_RED": 12, "LED_GREEN": 13},
}

"""
BOARDS generate with:

git clone https://github.com/platformio/platform-espressif8266
for x in platform-espressif8266/boards/*.json; do
  max_size=$(jq -r .upload.maximum_size <"$x")
  name=$(jq -r .name <"$x")
  fname=$(basename "$x")
  board="${fname%.*}"
  size_mb=$((max_size / (1024 * 1024)))
  if [[ $size_mb -gt 0 ]]; then
    size="${size_mb}_MB"
  else
    size="${$((max_size / 1024))}_KB"
  fi
  echo "    \"$board\": {\"name\": \"$name\", \"flash_size\": FLASH_SIZE_$size,},"
done | sort
"""

BOARDS = {
    "agruminolemon": {
        "name": "Lifely Agrumino Lemon v4",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "d1_mini_lite": {
        "name": "WeMos D1 mini Lite",
        "flash_size": FLASH_SIZE_1_MB,
    },
    "d1_mini": {
        "name": "WeMos D1 R2 and mini",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "d1_mini_pro": {
        "name": "WeMos D1 mini Pro",
        "flash_size": FLASH_SIZE_16_MB,
    },
    "d1": {
        "name": "WEMOS D1 R1",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "eduinowifi": {
        "name": "Schirmilabs Eduino WiFi",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "esp01_1m": {
        "name": "Espressif Generic ESP8266 ESP-01 1M",
        "flash_size": FLASH_SIZE_1_MB,
    },
    "esp01": {
        "name": "Espressif Generic ESP8266 ESP-01 512k",
        "flash_size": FLASH_SIZE_512_KB,
    },
    "esp07": {
        "name": "Espressif Generic ESP8266 ESP-07 1MB",
        "flash_size": FLASH_SIZE_1_MB,
    },
    "esp07s": {
        "name": "Espressif Generic ESP8266 ESP-07S",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "esp12e": {
        "name": "Espressif ESP8266 ESP-12E",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "esp210": {
        "name": "SweetPea ESP-210",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "esp8285": {
        "name": "Generic ESP8285 Module",
        "flash_size": FLASH_SIZE_1_MB,
    },
    "espduino": {
        "name": "ESPDuino (ESP-13 Module)",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "espectro": {
        "name": "ESPectro Core",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "espino": {
        "name": "ESPino",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "espinotee": {
        "name": "ThaiEasyElec ESPino",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "espmxdevkit": {
        "name": "ESP-Mx DevKit (ESP8285)",
        "flash_size": FLASH_SIZE_1_MB,
    },
    "espresso_lite_v1": {
        "name": "ESPresso Lite 1.0",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "espresso_lite_v2": {
        "name": "ESPresso Lite 2.0",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "esp_wroom_02": {
        "name": "ESP-WROOM-02",
        "flash_size": FLASH_SIZE_2_MB,
    },
    "gen4iod": {
        "name": "4D Systems gen4 IoD Range",
        "flash_size": FLASH_SIZE_512_KB,
    },
    "heltec_wifi_kit_8": {
        "name": "Heltec Wifi kit 8",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "huzzah": {
        "name": "Adafruit HUZZAH ESP8266",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "inventone": {
        "name": "Invent One",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "modwifi": {
        "name": "Olimex MOD-WIFI-ESP8266(-DEV)",
        "flash_size": FLASH_SIZE_2_MB,
    },
    "nodemcu": {
        "name": "NodeMCU 0.9 (ESP-12 Module)",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "nodemcuv2": {
        "name": "NodeMCU 1.0 (ESP-12E Module)",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "oak": {
        "name": "DigiStump Oak",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "phoenix_v1": {
        "name": "Phoenix 1.0",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "phoenix_v2": {
        "name": "Phoenix 2.0",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "sonoff_basic": {
        "name": "Sonoff Basic",
        "flash_size": FLASH_SIZE_1_MB,
    },
    "sonoff_s20": {
        "name": "Sonoff S20",
        "flash_size": FLASH_SIZE_1_MB,
    },
    "sonoff_sv": {
        "name": "Sonoff SV",
        "flash_size": FLASH_SIZE_1_MB,
    },
    "sonoff_th": {
        "name": "Sonoff TH",
        "flash_size": FLASH_SIZE_1_MB,
    },
    "sparkfunBlynk": {
        "name": "SparkFun Blynk Board",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "thingdev": {
        "name": "SparkFun ESP8266 Thing Dev",
        "flash_size": FLASH_SIZE_512_KB,
    },
    "thing": {
        "name": "SparkFun ESP8266 Thing",
        "flash_size": FLASH_SIZE_512_KB,
    },
    "wifiduino": {
        "name": "WiFiduino",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "wifinfo": {
        "name": "WifInfo",
        "flash_size": FLASH_SIZE_1_MB,
    },
    "wifi_slot": {
        "name": "WiFi Slot",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "wio_link": {
        "name": "Wio Link",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "wio_node": {
        "name": "Wio Node",
        "flash_size": FLASH_SIZE_4_MB,
    },
    "xinabox_cw01": {
        "name": "XinaBox CW01",
        "flash_size": FLASH_SIZE_4_MB,
    },
}
