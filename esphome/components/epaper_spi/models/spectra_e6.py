from typing import Any

from . import EpaperModel


class SpectraE6(EpaperModel):
    def __init__(self, name, class_name="EPaperSpectraE6", **defaults):
        super().__init__(name, class_name, **defaults)

    # fmt: off
    def get_init_sequence(self, config: dict):
        width, height = self.get_dimensions(config)
        return (
            (0xAA, 0x49, 0x55, 0x20, 0x08, 0x09, 0x18,),
            (0x01, 0x3F,),
            (0x00, 0x5F, 0x69,),
            (0x03, 0x00, 0x54, 0x00, 0x44,),
            (0x05, 0x40, 0x1F, 0x1F, 0x2C,),
            (0x06, 0x6F, 0x1F, 0x17, 0x49,),
            (0x08, 0x6F, 0x1F, 0x1F, 0x22,),
            (0x30, 0x03,),
            (0x50, 0x3F,),
            (0x60, 0x02, 0x00,),
            (0x61, width // 256, width % 256, height // 256, height % 256,),
            (0x84, 0x01,),
            (0xE3, 0x2F,),
        )

    def get_default(self, key, fallback: Any = False) -> Any:
        return self.defaults.get(key, fallback)


spectra_e6 = SpectraE6("spectra-e6", minimum_update_interval="30s")

spectra_e6_7p3 = spectra_e6.extend(
    "7.3in-Spectra-E6",
    width=800,
    height=480,
    data_rate="20MHz",
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
