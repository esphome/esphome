from esphome.components.mipi import delay

from . import EpaperModel


class T133A01(EpaperModel):
    def __init__(
        self,
        name: str,
        class_name: str = "EPaperT133A01",
        initsequence=(),
        **defaults,
    ):
        super().__init__(name, class_name, initsequence=tuple(initsequence), **defaults)

    # fmt: off
    def get_init_sequence(self, config: dict):
        # Sequence adapted from Seeed_GFX (T133A01_Defines.h / EPD_INIT).
        # Some commands need to be mirrored to the second controller (CS1) in C++.
        return (
            (0x74, 0xC0, 0x1C, 0x1C, 0xCC, 0xCC, 0xCC, 0x15, 0x15, 0x55),
            delay(10),
            (0xF0, 0x49, 0x55, 0x13, 0x5D, 0x05, 0x10),
            delay(10),
            (0x00, 0xDF, 0x69),
            delay(10),
            (0x50, 0x37),
            delay(10),
            (0x60, 0x03, 0x03),
            delay(10),
            (0x86, 0x10),
            delay(10),
            (0xE3, 0x22),
            delay(10),
            (0x61, 0x04, 0xB0, 0x03, 0x20),
            delay(10),
            (0x01, 0x0F, 0x00, 0x28, 0x2C, 0x28, 0x38),
            delay(10),
            (0xB6, 0x07),
            delay(10),
            (0x06, 0xD8, 0x18),
            delay(10),
            (0xB7, 0x01),
            delay(10),
            (0x05, 0xD8, 0x18),
            delay(10),
            (0xB0, 0x01),
            delay(10),
            (0xB1, 0x02),
            delay(10),
        )


t133a01 = T133A01(
    "T133A01",
    width=1200,
    height=1600,
    data_rate="10MHz",
    minimum_update_interval="30s",
    reset_duration="20ms",
)

# Pin defaults for Seeed Studio XIAO ePaper Display Board (EE02) + 13.3" six-color panel (T133A01)
# See manufacturer library: EPaper_Board_Pins_Setups.h (USE_XIAO_EPAPER_DISPLAY_BOARD_EE02)
# Note: BUSY is active-low on this board, so we invert it to match epaper_spi busy semantics.

t133a01.extend(
    "Seeed-XIAO-EPaper-13.3in",
    cs_pin=44,
    cs1_pin=41,
    dc_pin=10,
    busy_pin={
        "number": 4,
        "inverted": True,
        "mode": {
            "input": True,
            "pullup": True,
        },
    },
    reset_pin=38,
    enable_pin=43,
)
