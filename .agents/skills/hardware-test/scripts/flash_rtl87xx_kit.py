"""ltchiptool for the BW15 kit whose CH340 drives CEN (RTS) and PA00 (DTR).

A plain open asserts both lines and holds the chip in reset. Here every open keeps DTR high and RTS
released; the first open pulses RTS so the ROM comes up; after the command DTR low plus an RTS pulse
boots the app. The exit status is ltchiptool's; 2 means the flash succeeded but the boot pulse
failed, so the board is still in the ROM and needs a reset.

Usage: python flash_rtl87xx_kit.py flash write -d <port> firmware.uf2
"""

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


def port_argument(argv: list[str]) -> str | None:
    for i, a in enumerate(argv):
        if a in ("-d", "--device") and i + 1 < len(argv):
            return argv[i + 1]
        if a.startswith(("-d=", "--device=")):
            return a.split("=", 1)[1]
    return None


def boot_app(port: str) -> None:
    s = serial.Serial(port=None)
    s.port = port
    s.dtr = False
    s.rts = False
    _orig_open(s)
    s.rts = True
    time.sleep(0.1)
    s.rts = False
    s.close()


def main() -> int:
    rc = 1
    try:
        result = cli_entrypoint.main(args=sys.argv[1:], standalone_mode=False)
        rc = int(result) if isinstance(result, int) else 0
    finally:
        port = port_argument(sys.argv)
        if port:
            try:
                boot_app(port)
                print("[flash_rtl87xx_kit] application booted")
            except (serial.SerialException, OSError) as err:
                print(f"[flash_rtl87xx_kit] boot pulse failed: {err}", file=sys.stderr)
                if rc == 0:
                    rc = 2  # flashed, but the board is still in the ROM: press reset
    return rc


if __name__ == "__main__":
    sys.exit(main())
