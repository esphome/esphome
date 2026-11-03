from .gpio_common import ESP32PinSpec

ESP32_C2_PIN_SPEC = ESP32PinSpec(max_pin=20, strapping_pins=frozenset({8, 9}))
