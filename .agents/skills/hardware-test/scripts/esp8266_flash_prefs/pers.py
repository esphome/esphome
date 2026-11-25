"""Reset the isr-load board many times per kind and check what the restore_value globals came back as.

usage: pers.py PORT HOST OUT KINDS N
  KINDS  comma separated: api (restart button), pin (RTS pulse right after connecting), pinrand (RTS pulse 3 to 8 s
         after boot), pinsave (RTS pulse 0 to 30 ms after a preferences "Saving" line), hwdt and swdt (the hang buttons)
  N      trials per kind
Holds the serial port (log in OUT-cap.log) and appends one line per trial to OUT-results.txt: the last counter printed
before the reset, the restored counter, their difference, the boot counter step, the shadow global difference and the
magic value. Opening a CP2102 port can reset the board, so it waits 8 s first.
"""

import asyncio
import contextlib
import pathlib
import random
import re
import sys
import threading
import time

import serial

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from apiconn import connect  # noqa: E402
from lognorm import ANSI  # noqa: E402
from serial_capture import reset_pulse, write_line  # noqa: E402

port, host, out, kinds, n = (
    sys.argv[1],
    sys.argv[2],
    sys.argv[3],
    sys.argv[4].split(","),
    int(sys.argv[5]),
)
ser = serial.Serial()
ser.port, ser.baudrate, ser.timeout = port, 115200, 0.2
ser.dtr = False
ser.rts = False
ser.open()
log = pathlib.Path(out + "-cap.log").open("ab")  # noqa: SIM115
st = {"tick": None, "tick_before": None, "boot": None, "keys": None, "err": None}
boot_ev = asyncio.Event()
# Set by a pinsave trial: the reader resets the board 0 to 30 ms after the next "Saving" line
arm_save = threading.Event()
TICK_RE = re.compile(r"pers:\d+\]: tick (\d+)")
BOOT_RE = re.compile(
    r"BOOT boots=(\d+) counter=(\d+) shadow=(\d+) magic=(\w+) reason=(.*)$"
)


def reader(loop: asyncio.AbstractEventLoop) -> None:
    try:
        read_lines(loop)
    except (OSError, serial.SerialException) as e:
        st["err"] = e
        loop.call_soon_threadsafe(boot_ev.set)


def read_lines(loop: asyncio.AbstractEventLoop) -> None:
    buf = b""
    while True:
        d = ser.read(ser.in_waiting or 1)
        if not d:
            continue
        buf += d
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            line = line.rstrip(b"\r")
            write_line(log, b"", line)
            s = ANSI.sub("", line.decode(errors="replace"))
            if arm_save.is_set() and "preferences:" in s and "Saving" in s:
                arm_save.clear()
                time.sleep(random.uniform(0, 0.03))
                reset_pulse(ser)
            if m := TICK_RE.search(s):
                st["tick"] = int(m.group(1))
            if m := BOOT_RE.search(s):
                # after a hang no more ticks arrive; keep the last one seen before the boot line
                st["tick_before"] = st["tick"]
                st["boot"] = m.groups()
                loop.call_soon_threadsafe(boot_ev.set)


async def trial(kind: str, i: int, res, prev_boots: int | None) -> int | None:
    c = None
    if kind == "pinrand":
        await asyncio.sleep(random.uniform(3, 8))
        boot_ev.clear()
        reset_pulse(ser)
    elif kind == "pinsave":
        await asyncio.sleep(random.uniform(4, 8))
        boot_ev.clear()
        arm_save.set()
    else:
        c = await connect(host, tries=90)
        if st["keys"] is None:
            ents, _ = await c.list_entities_services()
            st["keys"] = {e.object_id: e.key for e in ents}
        await asyncio.sleep(random.uniform(5, 12))
        boot_ev.clear()
        if kind == "pin":
            reset_pulse(ser)
        else:
            c.button_command(st["keys"]["restart" if kind == "api" else kind])
    try:
        await asyncio.wait_for(boot_ev.wait(), 40)
        ok = True
    except TimeoutError:
        ok = False
    arm_save.clear()
    if st["err"] is not None:
        sys.exit(f"serial reader stopped: {st['err']}")
    if c:
        with contextlib.suppress(Exception):
            await c.disconnect(force=True)
    if not ok:
        print(f"{kind} {i}: no boot line", file=res, flush=True)
        return prev_boots
    boots, cnt, sh, magic, reason = st["boot"]
    last = st["tick_before"]
    step = "-" if prev_boots is None else int(boots) - prev_boots
    line = (
        f"{kind} {i}: last_tick={last} restored={cnt} diff={int(last) - int(cnt)} boots={boots}"
        f" boots_step={step} shadow_diff={int(cnt) - int(sh)} magic={magic} reason={reason}"
    )
    print(time.strftime("%T"), line, file=res, flush=True)
    print(line, flush=True)
    return int(boots)


async def main() -> None:
    threading.Thread(
        target=reader, args=(asyncio.get_running_loop(),), daemon=True
    ).start()
    prev_boots = None
    with pathlib.Path(out + "-results.txt").open("a", encoding="utf-8") as res:
        await asyncio.sleep(8)
        for kind in kinds:
            for i in range(n):
                prev_boots = await trial(kind, i, res, prev_boots)
                await asyncio.sleep(3)


asyncio.run(main())
