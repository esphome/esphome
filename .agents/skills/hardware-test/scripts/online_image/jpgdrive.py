"""Fetch images through the online_image API actions and report outcome, time and the loop time maximum per case.
usage: pngdrive.py HOST KEY LOGFILE BASEURL LABEL
"""

import asyncio
import contextlib
import pathlib
import re
import sys
import time

from aioesphomeapi import APIClient, LogLevel, SensorInfo, SensorState, UserService

HOST, KEY, LOGFILE, BASE, LABEL = sys.argv[1:6]
CAP_S = 60
HEAP_MIN = {"v": None}
OUTCOME = re.compile(r"download (finished|error): (\w+)")
INTEREST = re.compile(
    r"took a long time|Error|error|Download ended|No data received|headers do not fit|buffer_size|Downloading image|fully downloaded|allocat|not caching"
)

CASES = [
    ("noise 320x240 jpeg 69 KB", "fetch", "noise320.jpg", "finished"),
    ("noise 320x240 again", "fetch", "noise320.jpg", "finished"),
    ("noise 320x240 again", "fetch", "noise320.jpg", "finished"),
    ("noise 640x480 jpeg 233 KB", "fetch", "noise640.jpg", "finished"),
    ("noise 640x480 again", "fetch", "noise640.jpg", "finished"),
    ("gradient 640x480 jpeg 14 KB", "fetch", "gradient640.jpg", "finished"),
    ("4 KB buffer: noise 320x240", "fetch_small", "noise320.jpg", "finished"),
    ("4 KB buffer: noise 640x480", "fetch_small", "noise640.jpg", "finished"),
    ("4 KB buffer: 12 KB exif", "fetch_small", "bigexif320.jpg", "any"),
    ("64 KB buffer: 12 KB exif", "fetch", "bigexif320.jpg", "finished"),
    ("progressive jpeg", "fetch", "progressive320.jpg", "any"),
    ("no content-length", "fetch", "noclen_noise320.jpg", "finished"),
    ("chunked", "fetch", "chunked_noise320.jpg", "finished"),
    ("half with matching length", "fetch", "half_noise320.jpg", "error"),
    ("closed early", "fetch", "closed_early.jpg", "error"),
    ("stalled after a third", "fetch", "stall_noise320.jpg", "error"),
    ("empty", "fetch", "empty.jpg", "error"),
    ("noise 320x240 after errors", "fetch", "noise320.jpg", "finished"),
    ("refetch 304", "refetch", "", "finished"),
    ("ok png 16x16", "fetch", "ok.png", "finished"),
    ("ok bmp 16x16", "fetch", "ok.bmp", "finished"),
    ("4 KB buffer: noise 640x480 again", "fetch_small", "noise640.jpg", "finished"),
    ("4 KB buffer: half", "fetch_small", "half_noise320.jpg", "error"),
    ("4 KB buffer: stalled", "fetch_small", "stall_noise320.jpg", "error"),
    (
        "4 KB buffer: noise 320x240 after errors",
        "fetch_small",
        "noise320.jpg",
        "finished",
    ),
]


async def main() -> None:
    c = APIClient(HOST, 6053, None, noise_psk=KEY)
    await c.connect(login=True)
    ents, services = await c.list_entities_services()
    actions = {s.name: s for s in services if isinstance(s, UserService)}
    loop_key = next(
        e.key for e in ents if isinstance(e, SensorInfo) and e.name == "Loop Time"
    )
    heap_key = next(
        e.key for e in ents if isinstance(e, SensorInfo) and e.name == "Heap Free"
    )
    state = {"loop": [], "heap": None, "lines": []}

    def on_state(s) -> None:
        if isinstance(s, SensorState) and s.key == loop_key:
            state["loop"].append(s.state)
        elif isinstance(s, SensorState) and s.key == heap_key:
            state["heap"] = s.state
            if HEAP_MIN["v"] is None or s.state < HEAP_MIN["v"]:
                HEAP_MIN["v"] = s.state

    def on_log(msg) -> None:
        line = msg.message.decode(errors="replace")
        log.write(f"{time.strftime('%H:%M:%S')} {line}\n")
        state["lines"].append(line)

    with pathlib.Path(LOGFILE).open("w", encoding="utf-8") as log:
        c.subscribe_states(on_state)
        c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
        await asyncio.sleep(3)
        print(f"{LABEL}: heap at start {state['heap']}")
        rows = []
        for label, action, path, expect in CASES:
            img = "img_small" if action.endswith("_small") else "img"
            state["loop"].clear()
            state["lines"].clear()
            HEAP_MIN["v"] = None
            args = {} if action.startswith("refetch") else {"url": f"{BASE}/{path}"}
            t0 = time.monotonic()
            log.write(f">>> {label} {path}\n")
            try:
                await c.execute_service(actions[action], args)
            except Exception as exc:  # noqa: BLE001
                print(f"      api call failed ({exc!r}); reconnecting", flush=True)
                with contextlib.suppress(Exception):
                    await c.disconnect()
                await asyncio.sleep(10)
                await c.connect(login=True)
                ents, services = await c.list_entities_services()
                actions = {s.name: s for s in services if isinstance(s, UserService)}
                c.subscribe_states(on_state)
                c.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)
                await asyncio.sleep(2)
                state["lines"].clear()
                t0 = time.monotonic()
                await c.execute_service(actions[action], args)
            outcome = "no outcome"
            while time.monotonic() - t0 < CAP_S:
                await asyncio.sleep(0.25)
                for line in state["lines"]:
                    m = OUTCOME.search(line)
                    if m and m.group(2) == img:
                        outcome = m.group(1)
                if outcome != "no outcome":
                    break
            took = time.monotonic() - t0
            await asyncio.sleep(4.5)  # two loop time reports after the case
            notes = [
                ln for ln in state["lines"] if INTEREST.search(ln) and "Loop" not in ln
            ]
            loop_max = max(state["loop"]) if state["loop"] else float("nan")
            ok = outcome == expect or (expect == "any" and outcome != "no outcome")
            rows.append((label, path, expect, outcome, took, loop_max, ok))
            print(
                f"{'PASS' if ok else 'FAIL'} {label:30s} {path:18s} expected {expect:9s} got {outcome:10s} {took:5.1f}s loop max {loop_max:7.0f} ms heap min {HEAP_MIN['v']}",
                flush=True,
            )
            for n in notes[:8]:
                print(f"      {n.strip()}", flush=True)
        await c.disconnect()
    print(
        "\nSUMMARY",
        "all expectations met"
        if all(r[6] for r in rows)
        else "MISMATCHES: " + ", ".join(r[0] for r in rows if not r[6]),
    )


asyncio.run(main())
