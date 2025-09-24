from .ili import ILI9341

ILI9341.extend(
    "ESP32-2432S028",
    data_rate="40MHz",
    cs_pin=15,
    dc_pin=2,
)

models = {}
