from .gpio_common import ESP32PinSpec, unusable_errors

_ESP_SDIO_PINS = {
    6: "Flash Clock",
    7: "Flash Data 0",
    8: "Flash Data 1",
    11: "Flash Command",
}

ESP32_PIN_SPEC = ESP32PinSpec(
    max_pin=39,
    strapping_pins=frozenset({0, 2, 5, 12, 15}),
    error_pins={
        **{
            num: "This pin cannot be used on ESP32s and is already used by the flash interface"
            f" (function: {function})."
            " If you are using an ESP32 module that uses a different flash pin"
            " configuration (e.g. ESP32-PICO-V3-02), you can set"
            " 'ignore_pin_validation_error: true' to bypass this check."
            for num, function in _ESP_SDIO_PINS.items()
        },
        # These pins are not exposed in GPIO mux (reason unknown)
        # but they're missing from IO_MUX list in datasheet
        **unusable_errors("ESP32", (24, 28, 29, 30, 31)),
    },
    warning_pins={
        num: f"Pin {num} (9-10) might already be used by the flash interface in QUAD IO flash mode."
        for num in (9, 10)
    },
    input_only_pins=frozenset(range(34, 40)),
    input_only_label=" (34-39)",
    # Not in the digital hold mask and not an RTC pad, so the driver cannot hold it
    no_hold_pins=frozenset({20}),
)
