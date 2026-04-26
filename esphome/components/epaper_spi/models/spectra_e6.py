from typing import Any

from . import EpaperModel, ultrachip_resolution


class SpectraE6(EpaperModel):
    class_name = "EPaperSpectraE6"
    source_files = (
        "epaper_spi_4bpp.cpp",
        "epaper_spi_spectra_e6.cpp",
    )

    # fmt: off
    def get_init_sequence(self, config: dict):
        width, height = self.get_dimensions(config)
        return (
            *self.initsequence,
            # TRES
            ultrachip_resolution(width, height),
        )

    def get_default(self, key, fallback: Any = False) -> Any:
        return self.defaults.get(key, fallback)


# Display controllers

# No documentation for the Spectra E6 controller is available, but it appears
# to use a similar command set to several Solomon, Jadard, or Ultrachip controllers.
# fmt: off
spectra_e6 = SpectraE6(
    "spectra-e6",
    data_rate="20MHz",
    minimum_update_interval="30s",
    initsequence=(
        # Common values adapted from Waveshare example code (MIT license)
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
        # TRES is sent here in the example code
        # T_VDCS
        (0x84, 0x01,),
        # PWS
        (0xE3, 0x2F,),
    ),
)

# Display panels

# E Ink ED2208-CFA (EL036MF1)
# fmt: off
spectra_e6_3p6 = spectra_e6.extend(
    "Spectra6-3.6in",
    width=400,
    height=600,
    add_init_sequence=(
        # Values from Waveshare "3.6inch e-Paper (E)" example code (MIT license)
        # https://github.com/waveshareteam/e-Paper/blob/master/E-paper_Separate_Program/3.6inch_e-Paper_E
        # Example code sends (0x06, 0x6F, 0x1F, 0x17, 0x17) during init,
        # then sends this command each time the screen is turned on.
        (0x06, 0x6F, 0x1F, 0x16, 0x29,),
    ),
)

# Alias: Waveshare 3.6inch e-Paper (E) (sku 32651) / 3.6inch e-Paper HAT+ (E) (sku 32650)
waveshare_3p6in_e = spectra_e6_3p6.extend("Waveshare-3.6in-E")

# E Ink ED2208-DOA (EL040EF1)
# fmt: off
spectra_e6_4 = spectra_e6.extend(
    "Spectra6-4.0in",
    width=400,
    height=600,
    add_init_sequence=(
        # Values from Waveshare "4inch e-Paper (E)" example code (MIT license)
        # https://github.com/waveshareteam/e-Paper/blob/master/E-paper_Separate_Program/4inch_e-Paper_E
        # BTST2
        # Example code sends (0x06, 0x6F, 0x1F, 0x17, 0x17) during init,
        # then sends this command each time the screen is turned on.
        (0x06, 0x6F, 0x1F, 0x17, 0x27,),
    ),
)

# Alias: Waveshare 4inch e-Paper (E) / 4inch e-Paper HAT+ (E) (sku 27367)
waveshare_4in_e = spectra_e6_4.extend("Waveshare-4in-E")

# E Ink ED2208-GCA (EL073TF1)
# fmt: off
spectra_e6_7p3 = spectra_e6.extend(
    "Spectra6-7.3in",
    width=800,
    height=480,
    add_init_sequence=(
        # Values from Waveshare "7.3inch e-Paper (E)" example code (MIT license)
        # https://github.com/waveshareteam/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_7in3e.c
        # BTST2
        # Example code sets this command multiple times - during init, and again each time the screen is turned on.
        (0x06, 0x6F, 0x1F, 0x17, 0x49,),
    ),
)

# Alias: Waveshare 7.3inch e-Paper (E) (sku 27863) / 7.3inch e-Paper HAT (E) (sku 27875)
waveshare_7p3in_e = spectra_e6_7p3.extend("Waveshare-7.3in-E")

# Alias: Previous undocumented name for backwards compatibility
spectra_e6_7p3_alias = spectra_e6_7p3.extend("7.3in-Spectra-E6")

# Integrated display boards

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
