"""Waveshare 4-level-grayscale e-paper displays using the UC8179 controller.

Supported models:
- waveshare-7.5in-v2-4gray: 800x480 (7.5" V2 panel, EPD_7in5_V2), driven in
  4-gray mode with hybrid differential partial refresh.

The init sequence configures the panel for the FULL 4-gray refresh (OTP
waveform) and uploads the register LUTs used by the differential partial
refresh; the driver switches modes per update on top of this baseline.
Power-on is handled in the POWER_ON state after data transfer, so the state
machine's busy wait covers the power-on delay.
"""

from . import EpaperModel

# 60-byte register LUTs for the differential partial refresh, 10 groups of
# [VS, TP0..TP3, RP] each (only group 0 used). WW/KK/border get an all-zero
# waveform (physical no-op), so unchanged pixels — including grays — hold
# their level; only K<->W transitions are driven, VS phase 10b toward VDL
# (white) / 01b toward VDH (black), TP0=0x0C frames (~350 ms). TP0 is the
# one tunable: raise it toward 0x19 if transitions look faint.
_LUT_SIZE = 60


def _lut(*head: int) -> tuple[int, ...]:
    return tuple(head) + (0,) * (_LUT_SIZE - len(head))


_LUT_VCOM = _lut(0x00, 0x0C, 0x01, 0x00, 0x00, 0x01)
_LUT_NOOP = _lut()
_LUT_KW = _lut(0x80, 0x0C, 0x01, 0x00, 0x00, 0x01)
_LUT_WK = _lut(0x40, 0x0C, 0x01, 0x00, 0x00, 0x01)


class UC8179G4(EpaperModel):
    """EpaperModel for UC8179 panels driven in 4-gray + hybrid-partial mode."""

    def __init__(self, name, **defaults):
        super().__init__(name, "EPaperUC8179G4", **defaults)

    def get_init_sequence(self, config):
        return (
            # POWER SETTING: VGH/VGL +-20V, VDH/VDL +-15V (register LUTs and
            # the OTP 4-gray waveform both drive with these)
            (0x01, 0x07, 0x07, 0x3F, 0x3F),
            # PANEL SETTING: KW mode, LUT from OTP
            (0x00, 0x1F),
            # VCOM AND DATA INTERVAL SETTING
            (0x50, 0x10, 0x07),
            # BOOSTER SOFT START
            (0x06, 0x27, 0x27, 0x18, 0x17),
            # 4-gray OTP waveform select
            (0xE0, 0x02),
            (0xE5, 0x5F),
            # Register LUTs for the differential partial refresh
            (0x20, *_LUT_VCOM),
            (0x21, *_LUT_NOOP),  # WW: no-op
            (0x22, *_LUT_KW),
            (0x23, *_LUT_WK),
            (0x24, *_LUT_NOOP),  # KK: no-op
            (0x25, *_LUT_NOOP),  # border: no-op
        )


UC8179G4(
    "waveshare-7.5in-v2-4gray",
    width=800,
    height=480,
    data_rate="10MHz",
    # A full 4-gray refresh is ~2 s of waveform; partials ~0.5 s.
    minimum_update_interval="5s",
)
