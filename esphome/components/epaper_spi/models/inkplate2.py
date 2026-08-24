# Reference: https://github.com/SolderedElectronics/Inkplate-Arduino-library

from . import EpaperModel


class Inkplate2Model(EpaperModel):
    def __init__(self, name, class_name="EPaperInkplate2", **kwargs):
        super().__init__(name, class_name, **kwargs)

    def get_init_sequence(self, config: dict):
        width, height = self.get_dimensions(config)
        return (
            (0x04,),  # power on
            (
                0x00,  # panel setting
                0x0F,  # LUT from OTP
                0x89,  # temperature/boost/timing
            ),
            (
                0x61,  # resolution
                width,  # width: 1 byte
                height >> 8,  # height: 2 bytes, high byte first ...
                height & 0xFF,  # ... then low byte
            ),
            (
                0x50,  # VCOM and data interval
                0x77,
            ),
        )


# Native orientation is portrait (104x212); use `rotation: 90` for the board's landscape orientation.
inkplate2 = Inkplate2Model(
    "inkplate2",
    width=104,
    height=212,
    data_rate="10MHz",
    # A full 3-color refresh takes ~20s, so don't allow updates faster than that.
    minimum_update_interval="30s",
    # Default GPIO pins for the on-board Inkplate 2 wiring.
    reset_pin=19,
    dc_pin=33,
    cs_pin=15,
    busy_pin={
        "number": 32,
        "inverted": True,  # hardware: LOW=busy, HIGH=idle
        "mode": {
            "input": True,
            "pullup": True,
        },
    },
)
