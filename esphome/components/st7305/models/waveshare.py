from esphome.components.mipi import DriverChip, delay

# Waveshare ESP32-S3-RLCD-4.2, 4.2" reflective panel. The same controller command set is
# used by the 300x400 ST7306 panels. Values follow the vendor demo code, which is the sequence
# proven on this panel; the datasheet's referential sequence is close but not identical.
DriverChip(
    "WAVESHARE-400X300",
    width=400,
    height=300,
    orientation="LANDSCAPE",
    column_start=0x12,
    column_end=0x2A,
    row_start=0x00,
    row_end=0xC7,
    initsequence=(
        (0xD6, 0x17, 0x02),  # NVM load control
        (0xD1, 0x01),  # booster enable
        (0xC0, 0x11, 0x04),  # gate voltage
        (0xC1, 0x69, 0x69, 0x69, 0x69),  # VSHP
        (0xC2, 0x19, 0x19, 0x19, 0x19),  # VSLP
        (0xC4, 0x4B, 0x4B, 0x4B, 0x4B),  # VSHN
        (0xC5, 0x19, 0x19, 0x19, 0x19),  # VSLN
        (0xD8, 0x80, 0xE9),  # oscillator
        (0xB2, 0x02),  # frame rate
        (
            0xB3,
            0xE5,
            0xF6,
            0x05,
            0x46,
            0x77,
            0x77,
            0x77,
            0x77,
            0x76,
            0x45,
        ),  # HPM gate EQ
        (0xB4, 0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45),  # LPM gate EQ
        (0x62, 0x32, 0x03, 0x1F),  # gate timing
        (0xB7, 0x13),  # source EQ
        (0xB0, 0x64),  # gate line setting, 400 lines
        (0x11,),  # sleep out
        delay(200),
        (0xC9, 0x00),  # source voltage select
        (0x36, 0x48),  # MADCTL: MX=1, DO=1
        (0x3A, 0x11),  # 1-bit data format, 3 bytes per 24-pixel block
        (0xB9, 0x20),  # gamma, mono
        (0xB8, 0x29),  # panel setting
        (0x21,),  # display inversion on
        (0x2A, 0x12, 0x2A),  # column address window
        (0x2B, 0x00, 0xC7),  # row address window
        (0x35, 0x00),  # tearing effect line on
        (0xD0, 0xFF),  # auto power down
        (0x38,),  # high power mode
        (0x29,),  # display on
    ),
)
