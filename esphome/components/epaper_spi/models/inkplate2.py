from typing import Any

from . import EpaperModel


class Inkplate2Model(EpaperModel):
    def __init__(self, name, class_name="EPaperInkplate2", **kwargs):
        super().__init__(name, class_name, **kwargs)

    def get_init_sequence(self, config: dict):
        width, height = self.get_dimensions(config)
        # Inkplate 2 init sequence
        # Must power on FIRST (0x04), wait for busy, then configure
        # State machine will wait for busy after this sequence before data transfer
        return (
            (0x04,),  # Power on - MUST BE FIRST
            # After power on, state machine waits for busy pin before continuing
            (
                0x00,
                0x0F,
                0x89,
            ),  # Panel setting: LUT from OTP, temp sensor
            (
                0x61,
                width,
                height >> 8,
                height & 0xFF,
            ),  # Resolution setting
            (
                0x50,
                0x77,
            ),  # VCOM and data interval setting
        )

    def get_default(self, key, fallback: Any = False) -> Any:
        return self.defaults.get(key, fallback)


# Create base model with Inkplate 2 defaults
inkplate2 = Inkplate2Model(
    "inkplate2",
    width=104,
    height=212,
    data_rate="1MHz",
    # Default GPIO pins for Inkplate 2 hardware
    reset_pin=19,
    dc_pin=33,
    cs_pin=15,
    busy_pin={
        "number": 32,
        "inverted": True,  # Hardware: LOW=busy, HIGH=idle
        "mode": {
            "input": True,
            "pullup": True,
        },
    },
)
