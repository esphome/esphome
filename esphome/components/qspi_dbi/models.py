# Commands
SW_RESET_CMD = 0x01
SLEEP_OUT = 0x11
INVERT_OFF = 0x20
INVERT_ON = 0x21
ALL_ON = 0x23
WRAM = 0x24
MIPI = 0x26
DISPLAY_OFF = 0x28
DISPLAY_ON = 0x29
RASET = 0x2B
CASET = 0x2A
WDATA = 0x2C
TEON = 0x35
MADCTL_CMD = 0x36
PIXFMT = 0x3A
BRIGHTNESS = 0x51
SWIRE1 = 0x5A
SWIRE2 = 0x5B
PAGESEL = 0xFE


class DriverChip:
    chips = {}

    def __init__(self, name: str):
        name = name.upper()
        self.name = name
        self.chips[name] = self
        self.initsequence = []

    def cmd(self, c, *args):
        """
        Add a command sequence to the init sequence
        :param c: The command (8 bit)
        :param args: zero or more arguments (8 bit values)
        """
        self.initsequence.extend([c, len(args)] + list(args))

    def delay(self, ms):
        self.initsequence.extend([ms, 0xFF])


chip = DriverChip("RM67162")
chip.cmd(PIXFMT, 0x55)
chip.cmd(BRIGHTNESS, 0)

chip = DriverChip("RM690B0")
chip.cmd(PAGESEL, 0x20)
chip.cmd(MIPI, 0x0A)
chip.cmd(WRAM, 0x80)
chip.cmd(SWIRE1, 0x51)
chip.cmd(SWIRE2, 0x2E)
chip.cmd(PAGESEL, 0x00)
chip.cmd(0xC2, 0x00)
chip.delay(10)
chip.cmd(TEON, 0x00)

chip = DriverChip("AXS15231")
chip.cmd(0xBB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0xA5)
chip.cmd(0xC1, 0x33)
chip.cmd(0xBB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)

DriverChip("Custom")
