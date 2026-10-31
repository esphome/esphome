from .gpio_common import ESP32PinSpec, spi_psram_errors

ESP32_C61_PIN_SPEC = ESP32PinSpec(
    max_pin=29,
    strapping_pins=frozenset({8, 9}),
    usb_jtag_pins=frozenset({12, 13}),
    # GPIO14-17, GPIO19-21 are used for SPI flash/PSRAM
    error_pins=spi_psram_errors(
        "ESP32-C61",
        {
            14: "SPICS0",
            15: "SPICLK",
            16: "SPID",
            17: "SPIQ",
            19: "SPIWP",
            20: "SPIHD",
            21: "VDD_SPI",
        },
    ),
)
