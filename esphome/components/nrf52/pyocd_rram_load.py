"""Write Intel hex files into the RRAM of an nRF54L chip through a pyOCD debug probe.

pyOCD's flash algorithm for some nRF54L chips faults, so like Zephyr's OpenOCD support for these
chips this writes the image straight into RRAM over the debug port, with writes enabled in the
RRAM controller. Runs in the Python environment that has pyOCD, without ESPHome:

    python -I pyocd_rram_load.py TARGET RRAMC_BASE UNIQUE_ID FILE [FILE ...]
"""

import contextlib
import sys
import time

from intelhex import IntelHex
from pyocd.core.exceptions import TransferError  # pylint: disable=import-error
from pyocd.core.helpers import ConnectHelper  # pylint: disable=import-error

RRAMC_TASKS_COMMITWRITEBUF = 0x008
RRAMC_READY = 0x400
RRAMC_CONFIG = 0x500
RRAMC_CONFIG_WRITE = (
    0x101  # WEN, with a one-line write buffer (as Zephyr's OpenOCD script)
)

DEMCR = 0xE000EDFC
DEMCR_VC_CORERESET = 0x1
AIRCR = 0xE000ED0C
AIRCR_SYSRESETREQ = 0x05FA0004  # with the write key

CHUNK = 0x1000
TIMEOUT_S = 5.0


def _wait_ready(target, rramc: int) -> None:
    deadline = time.monotonic() + TIMEOUT_S
    while target.read32(rramc + RRAMC_READY) != 1:
        if time.monotonic() > deadline:
            raise RuntimeError("RRAM controller did not become ready")


def _start(target) -> None:
    """Start the new firmware with a system reset request, which keeps the debug link.

    A reset through the CTRL-AP faults every later debug transfer of the session.
    """
    target.write32(DEMCR, target.read32(DEMCR) & ~DEMCR_VC_CORERESET)
    # The chip may reset before the write is acknowledged
    with contextlib.suppress(TransferError):
        target.write32(AIRCR, AIRCR_SYSRESETREQ)
        target.flush()


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


def _flash(target, rramc: int, segments: list[tuple[int, bytes]]) -> bool:
    """Write the runs into RRAM; True when they read back the same."""
    total = sum(len(data) for _, data in segments)
    target.halt()  # connecting halts the core already; make sure
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
            return False
    print(f"Wrote and verified {total} bytes")
    return True


def main() -> int:
    target_type, rramc, unique_id, *files = sys.argv[1:]
    segments = _segments(files)
    session = ConnectHelper.session_with_chosen_probe(
        unique_id=unique_id or None,
        target_override=target_type,
        blocking=False,
        # The reset that starts the firmware already lets the core run
        options={"resume_on_disconnect": False},
    )
    if session is None:
        return 1  # pyOCD has printed why
    verified = False
    try:
        with session:
            if verified := _flash(session.target, int(rramc, 0), segments):
                _start(session.target)
    except TransferError:
        if not verified:
            raise
        # The reset can still fault the debug link while the session closes
    return 0 if verified else 1


if __name__ == "__main__":
    sys.exit(main())
