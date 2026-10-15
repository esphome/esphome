"""ltchiptool for boards whose RTS drives CEN: reset and link on one open port.

Stock ltchiptool closes and reopens the port after its DTR/RTS reset, and
pyserial raises both lines on open, holding CEN low through the BootROM's
listen window. Open with both lines released and pulse the reset right
before the LinkChecks instead.
"""

import sys

import bk7231tools.serial.main as bk_main
from ltchiptool.__main__ import cli
from ltchiptool.soc.bk72xx.flash import BK72XXFlash
from serial import Serial


class ReleasedSerial(Serial):
    def __init__(self, port=None, **kwargs):
        super().__init__(None, **kwargs)
        self.dtr = False
        self.rts = False
        self.port = port
        self.open()


bk_main.Serial = ReleasedSerial

_connect = BK72XXFlash.flash_connect


def flash_hw_reset(self) -> None:
    self._reset_pending = True


def flash_connect(self) -> None:
    if self.bk and self.conn.linked:
        return
    self.flash_build_protocol()
    if getattr(self, "_reset_pending", False):
        self._reset_pending = False
        self.bk.hw_reset()
    _connect(self)


BK72XXFlash.flash_hw_reset = flash_hw_reset
BK72XXFlash.flash_connect = flash_connect

sys.argv = ["ltchiptool", *sys.argv[1:]]
cli()
