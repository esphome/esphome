"""List the boards on USB, or wait for one to enumerate.

  ports.py                          table: port, vid:pid, product, serial number, guess
  ports.py --wait 2e8a              print the first port with VID 0x2e8a once it appears (default 60 s)
  ports.py --wait 1a86:7523 --timeout 30
  ports.py --product "USB Serial"   first port whose USB product string matches (case-insensitive)

Run with a Python that has pyserial (any ESPHome venv). Port names follow the hub port on macOS, so
re-run this after every replug instead of trusting a name from an earlier session.
"""

import argparse
import sys
import time

from serial.tools import list_ports

GUESS = [
    (
        (0x1A86, 0x7523),
        "CH340: D1 mini, BW15 kit, BK7238/CB3S or LN882H adapter; identify by the boot banner",
    ),
    ((0x1A86, 0x55D3), "CH343: ESP32-P4 devkit console"),
    ((0x1A86, 0x55D4), "CH343: ESP32 devkit"),
    ((0x1A86, None), "WCH CH34x USB serial bridge"),
    ((0x10C4, 0xEA60), "CP2102: classic ESP32 devkit (esp32dev)"),
    (
        (0x303A, 0x1001),
        "Espressif USB JTAG serial: S3/C3/C6/H2 native USB (logger hardware_uart USB_SERIAL_JTAG)",
    ),
    (
        (0x2E8A, None),
        "Raspberry Pi Pico W / Pico 2 W CDC; the port vanishes and comes back on every reboot",
    ),
    ((0x1915, 0x521F), "nRF52840 Dongle (PCA10059) in the Open DFU bootloader"),
    (
        (0x2FE3, 0x0100),
        "nRF52840 Dongle running a Zephyr app; two CDC ports, ESPHome log on the second",
    ),
    ((0x1915, 0xCAFE), "Makerdiary nRF52840 MDK dongle running a Zephyr app"),
    ((0x239A, None), "Adafruit style nRF52 board (UF2 bootloader family)"),
]


def guess(vid: int, pid: int) -> str:
    for (gv, gp), text in GUESS:
        if vid == gv and (gp is None or pid == gp):
            return text
    return ""


def parse_id(text: str) -> tuple[int, int | None]:
    vid, _, pid = text.partition(":")
    return int(vid, 16), (int(pid, 16) if pid else None)


def matches(port, vid: int | None, pid: int | None, product: str | None) -> bool:
    if vid is not None and (port.vid != vid or (pid is not None and port.pid != pid)):
        return False
    return not product or (port.product or "").lower() == product.lower()


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument(
        "--wait",
        metavar="VID[:PID]",
        help="block until a matching port appears, print it",
    )
    ap.add_argument("--product", help="USB product string to match")
    ap.add_argument("--timeout", type=float, default=60)
    args = ap.parse_args()

    vid = pid = None
    if args.wait:
        vid, pid = parse_id(args.wait)

    if args.wait or args.product:
        deadline = time.time() + args.timeout
        while True:
            for p in sorted(list_ports.comports(), key=lambda p: p.device):
                if p.vid and matches(p, vid, pid, args.product):
                    print(p.device)
                    return 0
            if time.time() >= deadline:
                print("no matching port", file=sys.stderr)
                return 1
            time.sleep(1)

    rows = [p for p in list_ports.comports() if p.vid]
    if not rows:
        print("no USB serial ports")
        return 1
    for p in sorted(rows, key=lambda p: p.device):
        print(
            f"{p.device:32} {p.vid:04x}:{p.pid:04x}  {p.product or '?':34} "
            f"{p.serial_number or '':18} {guess(p.vid, p.pid)}"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
