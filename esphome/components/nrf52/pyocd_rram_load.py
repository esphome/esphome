"""Write Intel hex files into the RRAM of an nRF54L chip through a pyOCD debug probe.

pyOCD's flash algorithm for some nRF54L chips faults, so like Zephyr's OpenOCD support for these
chips this writes the image straight into RRAM over the debug port, with writes enabled in the
RRAM controller. Runs in the Python environment that has pyOCD, without ESPHome:

    python -I pyocd_rram_load.py TARGET RRAMC_BASE UNIQUE_ID FILE [FILE ...]
"""

import sys
import time

from intelhex import IntelHex
from pyocd.core.helpers import ConnectHelper  # pylint: disable=import-error

RRAMC_TASKS_COMMITWRITEBUF = 0x008
RRAMC_READY = 0x400
RRAMC_CONFIG = 0x500
RRAMC_CONFIG_WRITE = (
    0x101  # WEN, with a one-line write buffer (as Zephyr's OpenOCD script)
)

DEMCR = 0xE000EDFC
DEMCR_VC_CORERESET = 0x1
CTRL_AP_RESET = 0x000

CHUNK = 0x1000
TIMEOUT_S = 5.0


def _wait_ready(target, rramc: int) -> None:
    deadline = time.monotonic() + TIMEOUT_S
    while target.read32(rramc + RRAMC_READY) != 1:
        if time.monotonic() > deadline:
            raise RuntimeError("RRAM controller did not become ready")


def _reset(target, halt: bool) -> None:
    """Reset the whole chip through the CTRL-AP; a plain core reset does not reach it."""
    demcr = target.read32(DEMCR)
    if halt:
        target.write32(DEMCR, demcr | DEMCR_VC_CORERESET)
    else:
        target.write32(DEMCR, demcr & ~DEMCR_VC_CORERESET)
    target.ctrl_ap.write_reg(CTRL_AP_RESET, 2)
    target.ctrl_ap.write_reg(CTRL_AP_RESET, 0)
    time.sleep(0.2)


def _segments(files: list[str]) -> list[tuple[int, bytes]]:
    """The data of all files as word-aligned (address, data) runs; gaps are not touched."""
    image = IntelHex()
    for file in files:
        image.merge(IntelHex(file), overlap="replace")
    return [
        (start, image.tobinstr(start=start, end=end - 1))
        for start, end in image.segments()
    ]


def _align(target, address: int, data: bytes) -> tuple[int, bytes]:
    """Widen a run to whole words, keeping the bytes around it as they are."""
    head = address % 4
    if head:
        address -= head
        data = bytes(target.read_memory_block8(address, head)) + data
    if tail := -len(data) % 4:
        data += bytes(target.read_memory_block8(address + len(data), tail))
    return address, data


def _words(data: bytes) -> list[int]:
    return [int.from_bytes(data[i : i + 4], "little") for i in range(0, len(data), 4)]


def main() -> int:
    target_type, rramc, unique_id, *files = sys.argv[1:]
    rramc = int(rramc, 0)
    segments = _segments(files)
    total = sum(len(data) for _, data in segments)
    session = ConnectHelper.session_with_chosen_probe(
        unique_id=unique_id or None,
        target_override=target_type,
        blocking=False,
    )
    if session is None:
        return 1  # pyOCD has printed why
    with session:
        target = session.target
        # Halt on the first instruction, before the old firmware can protect its RRAM
        _reset(target, halt=True)
        target.halt()
        target.write32(rramc + RRAMC_CONFIG, RRAMC_CONFIG_WRITE)
        written = 0
        try:
            for address, data in segments:
                address, data = _align(target, address, data)
                for offset in range(0, len(data), CHUNK):
                    chunk = data[offset : offset + CHUNK]
                    _wait_ready(target, rramc)
                    target.write_memory_block32(address + offset, _words(chunk))
                    written += len(chunk)
                    print(f"\rWriting {written * 100 // total}%", end="", flush=True)
            target.write32(rramc + RRAMC_TASKS_COMMITWRITEBUF, 1)
            _wait_ready(target, rramc)
        finally:
            target.write32(rramc + RRAMC_CONFIG, 0)
        print()
        for address, data in segments:
            if bytes(target.read_memory_block8(address, len(data))) != data:
                print(
                    f"Verify failed in the run at 0x{address:08x}: the RRAM is "
                    "write-protected or the chip is locked",
                    file=sys.stderr,
                )
                return 1
        print(f"Wrote and verified {total} bytes")
        _reset(target, halt=False)
        target.resume()
    return 0


if __name__ == "__main__":
    sys.exit(main())
