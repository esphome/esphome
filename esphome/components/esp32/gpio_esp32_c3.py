from .gpio_common import ESP32PinSpec, spi_psram_errors

ESP32_C3_PIN_SPEC = ESP32PinSpec(
    max_pin=21,
    strapping_pins=frozenset({2, 8, 9}),
    usb_jtag_pins=frozenset({18, 19}),
    error_pins=spi_psram_errors(
        "ESP32-C3",
        {
            12: "SPIHD",
            13: "SPIWP",
            14: "SPICS0",
            15: "SPICLK",
            16: "SPID",
            17: "SPIQ",
        },
    ),
)
