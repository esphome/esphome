"""ltchiptool for the BW15 kit whose CH340 drives CEN (RTS) and PA00 (DTR).
A plain open asserts both lines and holds the chip in reset. Here every open keeps DTR high and RTS released;
the first open pulses RTS so the ROM comes up; after the command DTR low plus an RTS pulse boots the app.
Usage: python flash_rtl87xx_kit.py flash write -d <port> firmware.uf2"""

import sys
import time

import serial

_first = [True]
_orig_open = serial.Serial.open


def _open(self):
    self.dtr = True  # PA00 high: ROM download mode on the next reset
    self.rts = False  # CEN released
    _orig_open(self)
    if _first[0]:
        _first[0] = False
        self.rts = True
        time.sleep(0.1)
        self.rts = False
        time.sleep(0.3)  # reset into the ROM
        self.reset_input_buffer()


serial.Serial.open = _open

from ltchiptool.__main__ import cli_entrypoint  # noqa: E402

try:
    cli_entrypoint.main(args=sys.argv[1:], standalone_mode=False)
finally:
    port = None
    for i, a in enumerate(sys.argv):
        if a in ("-d", "--device") and i + 1 < len(sys.argv):
            port = sys.argv[i + 1]
    if port:
        s = serial.Serial(port=None)
        s.port = port
        s.dtr = False
        s.rts = False
        _orig_open(s)
        s.rts = True
        time.sleep(0.1)
        s.rts = False  # boot the application
        s.close()
        print("[flash_rtl87xx_kit] application booted")
