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
log = pathlib.Path(out + "-cap.log").open("a", encoding="utf-8", errors="replace")  # noqa: SIM115
st = {"tick": None, "boot": None, "boot_seq": 0, "save_seq": 0}
ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")
BOOT_RE = re.compile(
    r"BOOT boots=(\d+) counter=(\d+) shadow=(\d+) magic=(\w+) reason=(.*)$"
)


def reader() -> None:
    buf = b""
    while True:
        d = ser.read(512)
        if not d:
            continue
        buf += d
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            s = ANSI.sub("", line.decode(errors="replace")).rstrip("\r")
            log.write(time.strftime("[%T]") + s + "\n")
            log.flush()
            if "preferences:" in s and "Saving" in s:
                st["save_seq"] += 1
            if m := re.search(r"pers:\d+\]: tick (\d+)", s):
                st["tick"] = int(m.group(1))
            if m := BOOT_RE.search(s):
                st["tick_before"] = st["tick"]
                st["boot"] = m.groups()
                st["boot_seq"] += 1


def pulse_reset() -> None:
    ser.dtr = False
    ser.rts = True
    time.sleep(0.15)
    ser.rts = False


async def wait_boot(seq: int, t: float = 40) -> bool:
    end = time.monotonic() + t
    while st["boot_seq"] == seq and time.monotonic() < end:
        await asyncio.sleep(0.1)
    return st["boot_seq"] != seq


async def trial(kind: str, i: int, res, prev_boots: int | None) -> int | None:
    c = None
    seq = st["boot_seq"]
    if kind == "pinrand":
        await asyncio.sleep(random.uniform(3, 8))
        pulse_reset()
    elif kind == "pinsave":
        await asyncio.sleep(random.uniform(4, 8))
        sv = st["save_seq"]
        while st["save_seq"] == sv:
            await asyncio.sleep(0.001)
        time.sleep(random.uniform(0, 0.03))
        pulse_reset()
    else:
        c = await connect(host, tries=90)
        ents, _ = await c.list_entities_services()
        keys = {e.object_id: e.key for e in ents}
        await asyncio.sleep(random.uniform(5, 12))
        seq = st["boot_seq"]
        if kind in ("api", "hwdt", "swdt"):
            c.button_command(keys["restart" if kind == "api" else kind])
        await asyncio.sleep(0 if kind == "pin" else 0.3)
        if kind == "pin":
            pulse_reset()
    last = st["tick"]
    ok = await wait_boot(seq)
    if c:
        with contextlib.suppress(Exception):
            await c.disconnect(force=True)
    if not ok:
        print(f"{kind} {i}: no boot line", file=res, flush=True)
        return prev_boots
    boots, cnt, sh, magic, reason = st["boot"]
    # after a hang no more ticks arrive; take the last one seen before the boot line
    last = st.get("tick_before", last)
    step = "-" if prev_boots is None else int(boots) - prev_boots
    line = (
        f"{kind} {i}: last_tick={last} restored={cnt} diff={int(last) - int(cnt)} boots={boots}"
        f" boots_step={step} shadow_diff={int(cnt) - int(sh)} magic={magic} reason={reason}"
    )
    print(time.strftime("%T"), line, file=res, flush=True)
    print(line, flush=True)
    return int(boots)


async def main() -> None:
    threading.Thread(target=reader, daemon=True).start()
    prev_boots = None
    with pathlib.Path(out + "-results.txt").open("a", encoding="utf-8") as res:
        await asyncio.sleep(8)
        for kind in kinds:
            for i in range(n):
                prev_boots = await trial(kind, i, res, prev_boots)
                await asyncio.sleep(3)


asyncio.run(main())
