from .gpio_common import ESP32PinSpec

ESP32_H2_PIN_SPEC = ESP32PinSpec(
    max_pin=27,
    strapping_pins=frozenset({8, 9, 25}),
    usb_jtag_pins=frozenset({26, 27}),
    warning_pins={
        num: f"GPIO{num} is reserved for SPI Flash communication on some ESP32-H2 chip variants.\n"
        "Utilizing SPI-reserved pins could cause unexpected failures.\n"
        "See https://docs.espressif.com/projects/esp-idf/en/latest/esp32h2/api-reference/peripherals/gpio.html"
        for num in (6, 7, 15, 16, 17, 18, 19, 20, 21)
    },
)
