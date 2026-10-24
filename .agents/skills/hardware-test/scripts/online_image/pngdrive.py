"""Fetch images through the online_image API actions and report outcome, time and the loop time maximum per case.
usage: pngdrive.py HOST KEY LOGFILE BASEURL LABEL
"""

import asyncio
import contextlib
import os
import pathlib
import re
import sys
import time

from aioesphomeapi import APIClient, LogLevel, SensorInfo, SensorState, UserService

HOST, KEY, LOGFILE, BASE, LABEL = sys.argv[1:6]
CAP_S = 60
OUTCOME = re.compile(r"download (finished|error): (\w+)")
INTEREST = re.compile(
    r"took a long time|Error|error|Download ended|No data received|Waiting for more|Decoded|cut off|already being"
)

CASES = [
    ("gradient 320x240 png", "fetch", "gradient.png", "finished"),
    ("gradient again", "fetch", "gradient.png", "finished"),
    ("gradient again", "fetch", "gradient.png", "finished"),
    ("flat 640x480 png", "fetch", "flat.png", "finished"),
    ("flat again", "fetch", "flat.png", "finished"),
    ("flat again", "fetch", "flat.png", "finished"),
    ("gradient jpeg", "fetch", "gradient.jpg", "finished"),
    ("ok bmp 16x16", "fetch", "ok.bmp", "finished"),
    ("ok png 16x16 body = gradient", "fetch", "ok.png", "finished"),
    ("chunked png", "fetch", "chunked.png", "finished"),
    ("chunked png cut in half", "fetch", "chunked_cut.png", "any"),
    ("closed early png", "fetch", "closed_early.png", "error"),
    ("stalled png", "fetch", "stall.png", "error"),
    ("slow png", "fetch", "slow.png", "finished"),
    ("empty png", "fetch", "empty.png", "error"),
    ("no content-length png", "fetch", "noclen.png", "any"),
    ("truncated bmp", "fetch", "truncated.bmp", "error"),
    ("gradient after errors", "fetch", "gradient.png", "finished"),
    ("gradient refetch 304", "refetch", "", "finished"),
    ("noise 320x240 png", "fetch", "noise.png", "finished"),
    ("noise again", "fetch", "noise.png", "finished"),
]

if os.environ.get("CASES") == "noise":
    CASES = [("noise 320x240 png", "fetch", "noise.png", "finished")] * 3

if os.environ.get("CASES") == "end":
    CASES = [
        (f"{pre}{img}", "fetch", f"{pre}{img}", exp)
        for img in ("flat.png", "gradient.png")
        for pre, exp in (
            ("", "finished"),
            ("noclen_", "finished"),
            ("chunked_", "finished"),
            ("half_", "any"),
            ("stall_", "error"),
        )
    ] + [("gradient after", "fetch", "gradient.png", "finished")]

if os.environ.get("CASES") == "bmpqoi":
    CASES = [
        (f"{pre}{img}", "fetch", f"{pre}{img}", exp)
        for img in (
            "bmp1_640.bmp",
            "bmp8_320.bmp",
            "bmp24_320.bmp",
            "qoi_flat.qoi",
            "qoi_check.qoi",
            "qoi_grad.qoi",
            "qoi_noise.qoi",
        )
        for pre, exp in (
            ("", "finished"),
            ("", "finished"),
            ("noclen_", "any"),
            ("chunked_", "any"),
            ("half_", "error"),
        )
    ] + [
        ("ok bmp 16x16", "fetch", "ok.bmp", "finished"),
        ("ok qoi 16x16", "fetch", "ok.qoi", "finished"),
        ("truncated bmp", "fetch", "truncated.bmp", "error"),
        ("stall_bmp1_640.bmp", "fetch", "stall_bmp1_640.bmp", "error"),
        ("stall_qoi_flat.qoi", "fetch", "stall_qoi_flat.qoi", "error"),
        ("qoi_flat after", "fetch", "qoi_flat.qoi", "finished"),
    ]

if os.environ.get("CASES") == "short":
    CASES = [
        ("gradient 320x240 png", "fetch", "gradient.png", "finished"),
        ("gradient again", "fetch", "gradient.png", "finished"),
        ("noise 320x240 png", "fetch", "noise.png", "finished"),
        ("noise again", "fetch", "noise.png", "finished"),
        ("noise again", "fetch", "noise.png", "finished"),
        ("gradient after noise", "fetch", "gradient.png", "finished"),
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
                f"{'PASS' if ok else 'FAIL'} {label:30s} {path:18s} expected {expect:9s} got {outcome:10s} {took:5.1f}s loop max {loop_max:7.0f} ms heap {state['heap']}",
                flush=True,
            )
            for n in notes[:6]:
                print(f"      {n.strip()}", flush=True)
        await c.disconnect()
    print(
        "\nSUMMARY",
        "all expectations met"
        if all(r[6] for r in rows)
        else "MISMATCHES: " + ", ".join(r[0] for r in rows if not r[6]),
    )


asyncio.run(main())
