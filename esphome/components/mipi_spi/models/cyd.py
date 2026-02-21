from .ili import ILI9341, ILI9342, ST7789V

ILI9341.extend(
    # ESP32-2432S028 CYD board with Micro USB, has ILI9341 controller
    "ESP32-2432S028",
    data_rate="40MHz",
    cs_pin={"number": 15, "ignore_strapping_warning": True},
    dc_pin={"number": 2, "ignore_strapping_warning": True},
)

ST7789V.extend(
    # ESP32-2432S028 CYD board with USB C + Micro USB, has ST7789V controller
    "ESP32-2432S028-7789",
    data_rate="40MHz",
    cs_pin={"number": 15, "ignore_strapping_warning": True},
    dc_pin={"number": 2, "ignore_strapping_warning": True},
)

# fmt: off

ILI9342.extend(
    # ESP32-2432S028 CYD board with USB C + Micro USB, has ILI9342 controller
    "ESP32-2432S028-9342",
    data_rate="40MHz",
    cs_pin={"number": 15, "ignore_strapping_warning": True},
    dc_pin={"number": 2, "ignore_strapping_warning": True},
    initsequence=(
        (0xCB, 0x39, 0x2C, 0x00, 0x34, 0x02),  # Power Control A
        (0xCF, 0x00, 0xC1, 0x30),  # Power Control B
        (0xE8, 0x85, 0x00, 0x78),  # Driver timing control A
        (0xEA, 0x00, 0x00),  # Driver timing control B
        (0xED, 0x64, 0x03, 0x12, 0x81),  # Power on sequence control
        (0xF7, 0x20),  # Pump ratio control
        (0xC0, 0x23),  # Power Control 1
        (0xC1, 0x10),  # Power Control 2
        (0xC5, 0x3E, 0x28),  # VCOM Control 1
        (0xC7, 0x86),  # VCOM Control 2
        (0xB1, 0x00, 0x1B),  # Frame Rate Control
        (0xB6, 0x0A, 0xA2, 0x27, 0x00),  # Display Function Control
        (0xF2, 0x00),  # Enable 3G
        (0x26, 0x01),  # Gamma Set
        (0xE0, 0x00, 0x0C, 0x11, 0x04, 0x11, 0x08, 0x37, 0x89, 0x4C, 0x06, 0x0C, 0x0A, 0x2E, 0x34, 0x0F),  # Positive Gamma Correction
        (0xE1, 0x00, 0x0B, 0x11, 0x05, 0x13, 0x09, 0x33, 0x67, 0x48, 0x07, 0x0E, 0x0B, 0x23, 0x33, 0x0F),  # Negative Gamma Correction
    )
)
