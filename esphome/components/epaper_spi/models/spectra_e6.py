from typing import Any

from . import EpaperModel


class SpectraE6(EpaperModel):
    def __init__(self, name, class_name="EPaperSpectraE6", **defaults):
        super().__init__(name, class_name, **defaults)

    # fmt: off
    def get_init_sequence(self, config: dict):
        width, height = self.get_dimensions(config)
        return (
            *self.initsequence,
            # TRES
            (0x61, width // 256, width % 256, height // 256, height % 256,),
        )

    def get_default(self, key, fallback: Any = False) -> Any:
        return self.defaults.get(key, fallback)

# No documentation for the Spectra E6 controller is available, but it appears
# to use a similar command set to the Solomon SPD1656 or Jadard JD79660.
spectra_e6 = SpectraE6(
    "spectra-e6",
    data_rate="20MHz",
    minimum_update_interval="30s",
    # Common values adapted from Waveshare example code (MIT license)
    initsequence=(
        # CMDH
        (0xAA, 0x49, 0x55, 0x20, 0x08, 0x09, 0x18,),
        # PSR
        (0x00, 0x5F, 0x69,),
        # PWR
        (0x01, 0x3F,),
        # PFS
        (0x03, 0x00, 0x54, 0x00, 0x44,),
        # BTST1
        (0x05, 0x40, 0x1F, 0x1F, 0x2C,),
        # BTST2 value is display specific.
        # The controller has a default which works but won't be optimal.
        # BTST3
        (0x08, 0x6F, 0x1F, 0x1F, 0x22,),
        # PLL
        (0x30, 0x08,),
        # CDI
        (0x50, 0x3F,),
        # TCON
        (0x60, 0x02, 0x00,),
        # T_VDCS
        (0x84, 0x01,),
        # PWS
        (0xE3, 0x2F,),
    )
)

spectra_e6_4 = spectra_e6.extend(
    "4.0in-Spectra-E6",
    width=400,
    height=600,
    add_init_sequence=(
        # Values from Waveshare "4inch e-Paper (E)" example code (MIT license)
        # https://github.com/waveshareteam/e-Paper/blob/master/E-paper_Separate_Program/4inch_e-Paper_E
        # BTST2
        # For this display, (0x6F, 0x1F, 0x17, 0x17) is used during init,
        # but updated to (0x6F, 0x1F, 0x17, 0x27) before doing a refresh.
        # I believe this allows for the power on to run faster.
        # Since there's no way to do this right now, set the final value here.
        (0x06, 0x6F, 0x1F, 0x17, 0x27,),
    )
)

spectra_e6_7p3 = spectra_e6.extend(
    "7.3in-Spectra-E6",
    width=800,
    height=480,
    add_init_sequence=(
        # Values from Waveshare "7.3inch e-Paper (E)" example code (MIT license)
        # https://github.com/waveshareteam/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_7in3e.c
        # BTST2
        (0x06, 0x6F, 0x1F, 0x17, 0x49,),
    )
)

spectra_e6_7p3.extend(
    "Seeed-reTerminal-E1002",
    cs_pin=10,
    dc_pin=11,
    reset_pin=12,
    busy_pin={
        "number": 13,
        "inverted": True,
        "mode": {
            "input": True,
            "pullup": True,
        },
    },
)
