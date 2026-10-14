# Reference: https://github.com/SolderedElectronics/Inkplate-Arduino-library (src/boards/Inkplate6COLOR)

from esphome.components.mipi import delay

from . import EpaperModel


class Inkplate6ColorModel(EpaperModel):
    def __init__(self, name, class_name="EPaperInkplate6Color", **kwargs):
        super().__init__(name, class_name, **kwargs)

    # fmt: off
    def get_init_sequence(self, config: dict):
        width, height = self.get_dimensions(config)
        return (
            (0x00, 0xEF, 0x08,),  # panel setting
            (0x01, 0x37, 0x00, 0x05, 0x05,),  # power setting
            (0x03, 0x00,),  # power off sequence setting
            (0x06, 0xC7, 0xC7, 0x1D,),  # booster soft start
            (0x41, 0x00,),  # temperature sensor enable
            (0x50, 0x37,),  # VCOM and data interval
            (0x60, 0x20,),  # TCON setting
            (0x61, width // 256, width % 256, height // 256, height % 256,),  # resolution set
            (0xE3, 0xAA,),  # power saving
            delay(100),
            (0x50, 0x37,),  # VCOM and data interval, resent once the power-saving setting settles
        )


# Native orientation is landscape (600x448).
inkplate6color = Inkplate6ColorModel(
    "inkplate6color",
    width=600,
    height=448,
    # Vendor library drives the panel at 2MHz; the controller doesn't reliably support faster rates.
    data_rate="2MHz",
    # Vendor library waits 200ms after releasing reset before talking to the panel.
    reset_duration="200ms",
    # A full 7-color refresh takes tens of seconds; disallow faster updates to avoid FSM update loops.
    minimum_update_interval="30s",
    # Panel's native buffer orientation is rotated 180 degrees relative to the logical
    # rotation=0 orientation; confirmed on real hardware.
    mirror_x=True,
    mirror_y=True,
    # Default GPIO pins for the on-board Inkplate 6COLOR wiring.
    reset_pin=19,
    dc_pin=33,
    cs_pin=27,
    busy_pin={
        "number": 32,
        "inverted": True,  # hardware: LOW=busy, HIGH=idle
        "mode": {
            "input": True,
            "pullup": True,
        },
    },
)
