# Reference implementation:
# https://github.com/SolderedElectronics/Inkplate-Arduino-library

from typing import Any

from . import EpaperModel


class Inkplate2Model(EpaperModel):
    def __init__(self, name, class_name="EPaperInkplate2", **kwargs):
        super().__init__(name, class_name, **kwargs)

    def get_init_sequence(self, config: dict):
        width, height = self.get_dimensions(config)
        # DEVIATION: Command 0x04 (power on) must be first in the init sequence for Inkplate 2.
        # Unlike the spectra_e6 e-paper displays where display power-on happens after data transfer, Inkplate 2
        # requires the display to be powered on before configuration. This matches Soldered's
        # reference implementation where setPanelDeepSleep(false) sends 0x04 first.
        # See: https://github.com/SolderedElectronics/Inkplate-Arduino-library/blob/master/src/boards/Inkplate2.cpp#L197-L212
        # The state machine will wait for busy after this sequence before proceeding to data transfer.
        return (
            (
                0x04,  # Power on / wake from deep sleep - MUST BE FIRST!
            ),
            (
                0x00,  # Enter panel setting
                0x0F,  # LUT from OTP 128x296
                0x89,  # Temperature sensor, boost and other related timing settings
            ),
            (
                0x61,  # Enter panel resolution setting
                width,
                height >> 8,
                height & 0xFF,
            ),
            (
                0x50,  # VCOM and data interval setting
                0x77,  # WBmode:VBDF 17|D7 VBDW 97 VBDB 57   WBRmode:VBDF F7 VBDW 77 VBDB 37  VBDR B7
            ),
        )

    def get_default(self, key, fallback: Any = False) -> Any:
        return self.defaults.get(key, fallback)


# Create base model with Inkplate 2 defaults
inkplate2 = Inkplate2Model(
    "inkplate2",
    width=104,
    height=212,
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
