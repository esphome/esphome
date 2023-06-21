RP2040_BASE_PINS = {}

RP2040_BOARD_PINS = {
    "pico": {
        "SDA": 4,
        "SCL": 5,
        "LED": 25,
        "SDA1": 26,
        "SCL1": 27,
    },
    "rpipico": "pico",
    "rpipicow": {
        "SDA": 4,
        "SCL": 5,
        "LED": 32,
        "SDA1": 26,
        "SCL1": 27,
    },
}

BOARDS = {
    "rpipico": {
        "name": "Raspberry Pi Pico",
    },
    "rpipicow": {
        "name": "Raspberry Pi Pico W",
    },
}
