from .gpio_common import ESP32PinSpec, spi_psram_errors, unusable_errors


def s2_s3_error_pins(variant: str) -> dict[int, str]:
    """Errors for the flash/PSRAM and missing pins, which S2 and S3 share."""
    return {
        **spi_psram_errors(
            variant,
            {
                26: "SPICS1",
                27: "SPIHD",
                28: "SPIWP",
                29: "SPICS0",
                30: "SPICLK",
                31: "SPIQ",
                32: "SPID",
            },
        ),
        # These pins are not exposed in GPIO mux (reason unknown)
        # but they're missing from IO_MUX list in datasheet
        **unusable_errors(variant, (22, 23, 24, 25)),
    }


ESP32_S2_PIN_SPEC = ESP32PinSpec(
    max_pin=46,
    strapping_pins=frozenset({0, 45, 46}),
    error_pins=s2_s3_error_pins("ESP32-S2"),
    input_only_pins=frozenset({46}),
)
