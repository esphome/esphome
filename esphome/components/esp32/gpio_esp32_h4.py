from .gpio_common import ESP32PinSpec

ESP32_H4_PIN_SPEC = ESP32PinSpec(
    max_pin=39,
    # Partial set from the ESP-IDF / esptool boot-mode docs:
    # https://docs.espressif.com/projects/esptool/en/latest/esp32h4/advanced-topics/boot-mode-selection.html
    # The full list awaits the ESP32-H4 datasheet's "Strapping Pins" section.
    strapping_pins=frozenset({13, 14}),
    usb_jtag_pins=frozenset({13, 14}),
)
